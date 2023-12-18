/**
 * manual compile : gcc main_profile_velocity_2.c -o ectest_PV -I/opt/etherlab/include -L/opt/etherlab/lib -lethercat -Wl,--rpath -Wl,/opt/etherlab/lib
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <time.h>
#include <sched.h>

#include "ecrt.h"

// **********************************************************

// Application parameters.
#define FREQUENCY 1000
#define CLOCK_TO_USE CLOCK_REALTIME
#define MEASURE_TIMING

#define NSEC_PER_SEC (1000000000L)
#define PERIOD_NS (NSEC_PER_SEC / FREQUENCY)

#define DIFF_NS(A, B) (((B).tv_sec - (A).tv_sec) * NSEC_PER_SEC + (B).tv_nsec - (A).tv_nsec)
#define TIMESPEC2NS(T) ((uint64_t)(T).tv_sec * NSEC_PER_SEC + (T).tv_nsec)

#ifdef MEASURE_TIMING
// Log tht save data.
char logname[50];
FILE *logfile = NULL;
int log_item_id = 1;
int num_of_timeouts = 0;
#endif

// quit flag.
volatile bool is_quit = false;

// Period.
const struct timespec cycletime = {0, PERIOD_NS};

// **********************************************************

// Operation model.
#define MOTOR_MODEL_PROFILE_VELOCITY 0x03 // Profile Velocity Mode.

// Control word.
#define MOTOR_MODEL_CONTROL_WORD_HALT 0x1 << 8 // Set motor halt.

// Profile_Acceleration.
#define MOTOR_PROFILE_ACCELERATION (10 * 1000) // 10s, unit: millisecond from 0 rpm to 3000 rpm.

// Profile_Deceleration.
#define MOTOR_PROFILE_DECELERATION (10 * 1000) // 10s, unit: millisecond from 0 rpm to 3000 rpm.

// Target velocity.
#define MOTOR_TARGET_VELOCITY_FORWARD 15000  // forwaed, 1500 rpm, uint: 0.1 rpm, maximum 50000.
#define MOTOR_TARGET_VELOCITY_REVERSE -15000 // reverse, 1500 rpm, uint: 0.1 rpm, maximum 50000.

// Slave num.
#define SLAVE_NUM 1

// PDO information of device, get from `sudo ethercat cstruct` in cmd line
/* Master 0, Slave 0
 * Vendor ID:       0x000001dd
 * Product code:    0x10305070
 * Revision number: 0x02040608
 */

// Slave alias, position, vendor ID and product ID.
#define SLAVE_0_ALIAS 0
#define SLAVE_0_POSITION 0
#define SLAVE_0_VID_PID 0x000001dd, 0x10305070

// Offsets for PDO entries.
static struct _SlaveOffset
{
    unsigned int Operation_Mode;
    unsigned int Ctrl_Word;
    unsigned int Target_Velocity;
    unsigned int Profile_Acceleration;
    unsigned int Profile_Deceleration;
    unsigned int Status_Word;
    unsigned int Current_Velocity;
} slave_offset[SLAVE_NUM];

struct _SlaveInfo
{
    uint32_t Alias;
    uint32_t Position;
    uint32_t Vendor_ID;
    uint32_t Product_Code;
};

struct _SlaveInfo slave_info[] = {{SLAVE_0_ALIAS, SLAVE_0_POSITION, SLAVE_0_VID_PID}};

struct _SlaveConfig
{
    // Slave configuration.
    ec_slave_config_t *sc;
    ec_slave_config_state_t sc_state;

    struct _SlaveOffset offset;
    int current_velocity;
};

struct _Domain
{
    ec_domain_t *domain;
    ec_domain_state_t domain_state;

    // Domain process data.
    uint8_t *domain_pd;
};

// Configure the PDOs entries register
const static ec_pdo_entry_reg_t domain_regs[] = {
    // Slave 0.
    {SLAVE_0_ALIAS, SLAVE_0_POSITION, SLAVE_0_VID_PID, 0x6040, 0, &slave_offset[0].Ctrl_Word},
    {SLAVE_0_ALIAS, SLAVE_0_POSITION, SLAVE_0_VID_PID, 0x6060, 0, &slave_offset[0].Operation_Mode},
    {SLAVE_0_ALIAS, SLAVE_0_POSITION, SLAVE_0_VID_PID, 0x60FF, 0, &slave_offset[0].Target_Velocity},
    {SLAVE_0_ALIAS, SLAVE_0_POSITION, SLAVE_0_VID_PID, 0x6083, 0, &slave_offset[0].Profile_Acceleration},
    {SLAVE_0_ALIAS, SLAVE_0_POSITION, SLAVE_0_VID_PID, 0x6084, 0, &slave_offset[0].Profile_Deceleration},
    {SLAVE_0_ALIAS, SLAVE_0_POSITION, SLAVE_0_VID_PID, 0x6041, 0, &slave_offset[0].Status_Word},
    {SLAVE_0_ALIAS, SLAVE_0_POSITION, SLAVE_0_VID_PID, 0x606B, 0, &slave_offset[0].Current_Velocity},
    {}};

ec_pdo_entry_info_t slave_pdo_entries[] = {
    {0x6040, 0x00, 16}, // Control word.
    {0x6060, 0x00, 8},  // Operation mode.
    {0x60FF, 0x00, 32}, // Target velocity.
    {0x6083, 0x00, 32}, // Profile acceleration.
    {0x6084, 0x00, 32}, // Profile deceleeration.
    {0x6041, 0x00, 16}, // Status word.
    {0x606B, 0x00, 32}, // Current velocity.
};

ec_pdo_info_t slave_pdos[] = {
    {0x1600, 5, slave_pdo_entries + 0},
    {0x1a00, 2, slave_pdo_entries + 5},
};

ec_sync_info_t slave_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
    {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
    {2, EC_DIR_OUTPUT, 1, slave_pdos + 0, EC_WD_DISABLE},
    {3, EC_DIR_INPUT, 1, slave_pdos + 1, EC_WD_DISABLE},
    {0xff}};

// Define EtherCAT master and corresponding states.
static ec_master_t *master = NULL;
static ec_master_state_t master_state = {};

// Define process data of domain
struct _Domain domain1;

// Check process data state.
void check_domain_state(struct _Domain *domain)
{
    ec_domain_state_t ds;
    ecrt_domain_state(domain->domain, &ds);

    if (ds.working_counter != domain->domain_state.working_counter)
    {
        /* Periodically check the read/write status of the slave memory, and return 'working_counter + i' if it can be read and write.
         * send read-only command(xRD), if slave memory readable, 'working_counter + 1';
         * send write-only command(xWR), if slave memory writable, 'working_counter + 1';
         * send read-write command(xRW), if slave memory readable, 'working_counter + 1', if slave memory writable, 'working_counter + 2', that means 'working_counter + 3' ;
         */
        printf("Domain: WC %u.\n", ds.working_counter);
    }

    if (ds.wc_state != domain->domain_state.wc_state)
    {
        /*
         * 0: No registered process data were exchanged;
         * 1: Some of the registered process data were exchanged.
         * 2: All registered process data were exchanged.
         */
        printf("Domain: State %u.\n", ds.wc_state);
    }

    domain->domain_state = ds;
}

// Check for master state.
void check_master_state()
{
    ec_master_state_t ms;
    ecrt_master_state(master, &ms);

    if (ms.slaves_responding != master_state.slaves_responding)
    {
        // Sum of responding slaves on all Ethernet devices.
        printf("%u slave(s).\n", ms.slaves_responding);
    }

    if (ms.al_states != master_state.al_states)
    {
        // Application-layer states of all slaves. 1:init; 2:preop; 4:safeop; 8:op;
        printf("AL states: 0x%02X.\n", ms.al_states);
    }

    if (ms.link_up != master_state.link_up)
    {
        printf("Link is %s.\n", ms.link_up ? "up" : "down");

        if (!ms.link_up)
        {
            is_quit = true;
        }
    }

    master_state = ms;
}

// Check for slave configuration state.
void check_slave_config_state(struct _SlaveConfig *slave_config)
{
    ec_slave_config_state_t s;
    int i;

    for (i = 0; i < SLAVE_NUM; i++)
    {
        memset(&s, 0, sizeof(s));

        ecrt_slave_config_state(slave_config[i].sc, &s);

        if (s.al_state != slave_config[i].sc_state.al_state)
        {
            // The application-layer state of the slave. 1:init; 2:preop; 4:safeop; 8:op;
            printf("slave[%d]: State 0x%02X.\n", i, s.al_state);
        }

        if (s.online != slave_config[i].sc_state.online)
        {
            printf("slave[%d]: %s.\n", i, s.online ? "online" : "offline");
        }

        if (!s.online)
        {
            is_quit = true;
        }

        if (s.operational != slave_config[i].sc_state.operational)
        {
            // The slave was brought into \a OP state using the specified configuration.
            printf("slave[%d]: %soperational.\n", i, s.operational ? "" : "Not ");
        }

        slave_config[i].sc_state = s;
    }
}

struct timespec timespec_add(struct timespec time1, struct timespec time2)
{
    struct timespec result;

    if ((time1.tv_nsec + time2.tv_nsec) >= NSEC_PER_SEC)
    {
        result.tv_sec = time1.tv_sec + time2.tv_sec + 1;
        result.tv_nsec = time1.tv_nsec + time2.tv_nsec - NSEC_PER_SEC;
    }
    else
    {
        result.tv_sec = time1.tv_sec + time2.tv_sec;
        result.tv_nsec = time1.tv_nsec + time2.tv_nsec;
    }

    return result;
}

// ************************************************************************

void cyclic_task(struct _SlaveConfig *slave_config, struct _Domain *domain)
{
    // Used to determine the value of the status word.
    uint16_t proof_value = 0x004F;
    uint16_t ctrl_word[] = {0x0006, 0x0007, 0x000f};
    uint16_t fault_reset = 0x008f;
    uint16_t status;
    uint32_t count = 0;
    static int direction = 0;

    struct timespec wakeupTime;

#ifdef MEASURE_TIMING
    struct timespec startTime, endTime, lastStartTime = {};
    uint32_t period_ns = 0, exec_ns = 0, latency_ns = 0, latency_min_ns = 0, latency_max_ns = 0,
             period_min_ns = 0, period_max_ns = 0, exec_min_ns = 0, exec_max_ns = 0, period_max_print = 0;
#endif
    int i;
    // Get current time.
    clock_gettime(CLOCK_TO_USE, &wakeupTime);

    while (!is_quit)
    {
        // Period time: 1ms.
        wakeupTime = timespec_add(wakeupTime, cycletime);
        clock_nanosleep(CLOCK_TO_USE, TIMER_ABSTIME, &wakeupTime, NULL);

#ifdef MEASURE_TIMING
        clock_gettime(CLOCK_TO_USE, &startTime);

        // The time interval of receiving and sending EtherCAT data.
        latency_ns = DIFF_NS(wakeupTime, startTime);

        // EtherCAT communication cycle.
        period_ns = DIFF_NS(lastStartTime, startTime);

        // Wait time for wake up.
        exec_ns = DIFF_NS(lastStartTime, endTime);

        lastStartTime = startTime;
#endif

        ecrt_master_receive(master);

        ecrt_domain_process(domain->domain);

        check_domain_state(domain);

        check_master_state();

        check_slave_config_state(slave_config);

#ifdef MEASURE_TIMING

        latency_max_ns = (latency_ns > latency_max_ns) ? latency_ns : latency_max_ns;
        latency_min_ns = (latency_ns < latency_min_ns) ? latency_ns : latency_min_ns;

        period_max_ns = (period_ns > period_max_ns) ? period_ns : period_max_ns;
        period_min_ns = (period_ns < period_min_ns) ? period_ns : period_min_ns;

        exec_max_ns = (exec_ns > exec_max_ns) ? exec_ns : exec_max_ns;
        exec_min_ns = (exec_ns < exec_min_ns) ? exec_ns : exec_min_ns;

        // Do not print frequently, or it will affect real-time performance.
        count++;
        if (count == FREQUENCY)
        {
            // Count the number of times the period exceeds 1100000ns and avoid the impact within the first second.
            if (log_item_id > 1 && period_max_ns >= 1100000)
                num_of_timeouts++;

            // output timing stats
            printf("period\tmin / max = %u / %u(ns)\n", period_min_ns, period_max_ns);
            printf("exec\tmin / max = %u / %u(ns)\n", exec_min_ns, exec_max_ns);
            printf("latency\tmin / max = %u / %u(ns)\n\n", latency_min_ns, latency_max_ns);

            if ((log_item_id++) > 1)
            {
                period_max_print = (period_max_print > period_max_ns) ? period_max_print : period_max_ns;
                printf("number of timeouts=%d, maximum period=%u\n\n", num_of_timeouts, period_max_print);
            }

            fprintf(logfile, "%d,%u,%u,%u,%u,%u,%u\r\n", log_item_id, period_min_ns, period_max_ns, exec_min_ns, exec_max_ns, latency_min_ns, latency_max_ns);

            period_max_ns = 0;
            period_min_ns = 0xffffffff;
            exec_max_ns = 0;
            exec_min_ns = 0xffffffff;
            latency_max_ns = 0;
            latency_min_ns = 0xffffffff;

            count = 0;
        }
#endif

        for (i = SLAVE_NUM - 1; i >= 0; i--)
        {
            // Read state word.
            status = EC_READ_U16(domain->domain_pd + slave_offset[i].Status_Word);
            switch (status & proof_value)
            {
            // Control phase 1.
            case 0x0040:
                // Set operation mode to velocity Mode.
                EC_WRITE_U8(domain->domain_pd + slave_offset[i].Operation_Mode, MOTOR_MODEL_PROFILE_VELOCITY);
                // Set Profile_Acceleration and Profile_Deceleration.
                EC_WRITE_U32(domain->domain_pd + slave_offset[i].Profile_Acceleration, MOTOR_PROFILE_ACCELERATION);
                EC_WRITE_U32(domain->domain_pd + slave_offset[i].Profile_Deceleration, MOTOR_PROFILE_DECELERATION);
                EC_WRITE_U16(domain->domain_pd + slave_offset[i].Ctrl_Word, ctrl_word[0]);
                proof_value = 0x006F;
                break;

            // Control phase 2.
            case 0x0021:
                EC_WRITE_U16(domain->domain_pd + slave_offset[i].Ctrl_Word, ctrl_word[1]);
                proof_value = 0x006F;
                break;

            // Control phase 3.
            case 0x0023:
                EC_WRITE_U16(domain->domain_pd + slave_offset[i].Ctrl_Word, ctrl_word[2]);
                proof_value = 0x006F;
                break;

            // Control phase 3.
            case 0x0027:
                slave_config[i].current_velocity = EC_READ_U32(domain->domain_pd + slave_offset[i].Current_Velocity);
                // Determine whether to turn forward or reverse.
                if (direction == 0)
                {
                    // forward.
                    if (slave_config[i].current_velocity == 0)
                    {
                        EC_WRITE_S32(domain->domain_pd + slave_offset[i].Target_Velocity, MOTOR_TARGET_VELOCITY_FORWARD);
                    }
                    else if (slave_config[i].current_velocity == MOTOR_TARGET_VELOCITY_FORWARD)
                    {
                        EC_WRITE_S32(domain->domain_pd + slave_offset[i].Target_Velocity, 0);
                        direction = 1;
                    }
                }
                else if (direction == 1)
                {
                    // reverse.
                    if (slave_config[i].current_velocity == 0)
                    {
                        EC_WRITE_S32(domain->domain_pd + slave_offset[i].Target_Velocity, MOTOR_TARGET_VELOCITY_REVERSE);
                    }
                    else if (slave_config[i].current_velocity == MOTOR_TARGET_VELOCITY_REVERSE)
                    {
                        EC_WRITE_S32(domain->domain_pd + slave_offset[i].Target_Velocity, 0);
                        direction = 0;
                    }
                }
                break;

            default:
                // Fault reset.
                EC_WRITE_U16(domain->domain_pd + slave_offset[i].Ctrl_Word, 0x008f);
                break;
            }
        }

        // Send process data.
        ecrt_domain_queue(domain->domain);
        ecrt_master_send(master);

#ifdef MEASURE_TIMING
        clock_gettime(CLOCK_TO_USE, &endTime);
#endif
    }

    printf("Exit cyclic task.\n");
}

// Function stop servo.
void stop_servo()
{
    int i;
    // Receive process data.
    ecrt_master_receive(master);
    ecrt_domain_process(domain1.domain);

    for (i = 0; i < SLAVE_NUM; i++)
    {
        EC_WRITE_U16(domain1.domain_pd + slave_offset[i].Ctrl_Word, MOTOR_MODEL_CONTROL_WORD_HALT);
    }

    // Send process data, queues all domain datagrams in the master's datagram queue.
    ecrt_domain_queue(domain1.domain);
    // Sends all datagrams in the queue.
    ecrt_master_send(master);
}

// Function exit program.
static void sig_handle(int signal)
{
    is_quit = true;

    stop_servo();

    fclose(logfile);
    printf("The log file is saved as %s\n", logname);

    printf("Manually exit the program!\n");
}

// Function initial the ethercat.
int init_ethercat(struct _SlaveConfig *slave_config, int *ret, int *status)
{
    int i;

    // Request EtherCAT master.
    master = ecrt_request_master(0);
    if (!master)
    {
        printf("Request master failed.\n");
        return -1;
    }
    printf("Request master success.\n");

    memset(&domain1, 0, sizeof(domain1));

    // Create domain.
    domain1.domain = ecrt_master_create_domain(master);
    if (!domain1.domain)
    {
        *status = -1;
        printf("Create domain failed.\n");
        goto err_leave;
    }
    printf("Create domain success.\n");

    // Get slave configuration.
    for (i = 0; i < SLAVE_NUM; i++)
    {
        slave_config[i].sc = ecrt_master_slave_config(master, slave_info[i].Alias,
                                                      slave_info[i].Position, slave_info[i].Vendor_ID,
                                                      slave_info[i].Product_Code);
        if (!slave_config[i].sc)
        {
            *status = -1;
            printf("Get slave configuration failed.\n");
            goto err_leave;
        }
    }
    printf("Get slave configuration success.\n");

    // Configuring PDO.
    for (i = 0; i < SLAVE_NUM; i++)
    {
        *ret = ecrt_slave_config_pdos(slave_config[i].sc, EC_END, slave_syncs);
        if (*ret != 0)
        {
            *status = -1;
            printf("Configuration PDO failed.\n");
            goto err_leave;
        }
    }
    printf("Configuration PDO success.\n");

    // Registers a bunch of PDO entries for a domain.
    *ret = ecrt_domain_reg_pdo_entry_list(domain1.domain, domain_regs);
    if (*ret != 0)
    {
        *status = -1;
        printf("Failed to register bunch of PDO entries for domain.\n");
        goto err_leave;
    }
    printf("Success to register bunch of PDO entries for domain.\n");

    // Activate EtherCAT master.
    *ret = ecrt_master_activate(master);
    if (*ret < 0)
    {
        *status = -1;
        printf("Activate master failed.\n");
        goto err_leave;
    }
    printf("Activate master success.\n");

    // Get Pointer to the process data memory.
    domain1.domain_pd = ecrt_domain_data(domain1.domain);
    if (!domain1.domain_pd)
    {
        *status = -1;
        printf("Get pointer to the process data memory failed.\n");
        goto err_leave;
    }
    printf("Get pointer to the process data memory success.\n");

    return *status;

// Function error leaving program.
err_leave:
    // Releases EtherCAT master.
    ecrt_release_master(master);
    printf("Release master.\n");
#ifdef MEASURE_TIMING
    fclose(logfile);
#endif
   return *status;
}

int main(int argc, char **argv)
{
    int status = 0, ret = -1;

    struct _SlaveConfig slave_config[SLAVE_NUM];

    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1)
    {
        perror("mlockall failed");
        return -1;
    }

    // Ctrl+c handler.
    signal(SIGINT, sig_handle);

    memset(&slave_config, 0, sizeof(slave_config));

#ifdef MEASURE_TIMING
    // Create a .csv file named by time.
    time_t current_time;
    struct tm *time_info;
    char time_buffer[20];
    time(&current_time);
    time_info = localtime(&current_time);
    strftime(time_buffer, sizeof(time_buffer), "%Y%m%d%H%M%S", time_info);

    snprintf(logname, sizeof(logname), "%s.csv", time_buffer);

    logfile = fopen(logname, "w");

    if (logfile == NULL)
    {
        perror("Error opening file");
        return 1;
    }

    // Defining List headers.
    fwrite("ID, period_min,period_max,exec_min,exec_max,latency_min,latency_max,\r\n", 1, strlen("ID, period_min,period_max,exec_min,exec_max,latency_min,latency_max,\r\n"), logfile);
#endif

    // init ethercat
    if (init_ethercat(slave_config, &ret, &status) == -1)
    {
        perror("init ethercat fail.");
        exit(EXIT_FAILURE);
    }

    // Set highest priority.
    pid_t pid = getpid();

    struct sched_param param;

    int max_priority = sched_get_priority_max(SCHED_FIFO);

    int min_priority = sched_get_priority_min(SCHED_FIFO);

    param.sched_priority = max_priority;

    if (sched_setscheduler(pid, SCHED_FIFO, &param) == -1)
    {
        perror("sched_setscheduler");
        exit(EXIT_FAILURE);
    }

    printf("Max priority: %d\n", max_priority);

    printf("Min priority: %d\n", min_priority);

    printf("Current priority: %d\n", sched_get_priority_max(SCHED_FIFO));

    // Start cyclic function.
    printf("Run cycle task...\n");
    cyclic_task(slave_config, &domain1);

    return status;
}
