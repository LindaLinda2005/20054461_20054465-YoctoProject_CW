#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <modbus/modbus.h>
#include <unistd.h>
#include <string.h>

#define SERVER_IP "192.168.137.100"     // Server IP
#define SERVER_PORT 1502                // Modbus TCP port
#define CPU_TEMP 0                      // Modbus register address to write/read
#define START_STOP 1                    // Modbus register address to write/read

// Function to read CPU temp
int get_cpu_temp(int debugTrue) {
    // DEBUG
    if (debugTrue == 1) {
        printf("DEBUG: calling 'get_cpu_temp'\n");
    }

    // Defining variables
    FILE *fp;
    int cpu_temp;
    char buffer[16];

    // Open file storing temperature data
    fp = popen("cat /sys/class/thermal/thermal_zone0/temp", "r");
    if (fp == NULL) {
        perror("Failed to run command\n");
        return -1; // Return error code
    }

    // If loop stores value of buffer into int
    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        cpu_temp = atoi(buffer);
    } else {
        perror("Failed to parse CPU temp\n");
        pclose(fp);
        return -1;
    }

    // Close the file & exit
    pclose(fp);
    return cpu_temp;
}

// Function to monitor CPU_temp
int monitor_cpu_temp(uint16_t cpu_temp[1], int *time_cpu, int debugTrue, int target) {
    // DEBUG
    if (debugTrue == 1) {
        printf("DEBUG: calling 'monitor_cpu_temp'\n");
    }

    // if cpu temp is high it will increase a timer, else timer will decrease
    if (cpu_temp[0] > target) {
        // DEBUG
        if (debugTrue == 1) {
            printf("DEBUG: CPU temp too high, time_cpu: %d\n", *time_cpu);
        }
        if (*time_cpu >= 10) {
            *time_cpu = 10;
        } else {
            (*time_cpu)++;
        }
    } else if (cpu_temp[0] < target && *time_cpu > 0) {
        (*time_cpu)--;
    }


    // DEBUG
    if (debugTrue == 1) {
        printf("DEBUG: time_cpu: %d\n", *time_cpu);
    }

    // Checks if cpu temp has been too high for too long
    int cpu_state = 0;
    static int was_high;

    // Static int used to provide hysteresis
    if (*time_cpu >= 3) {
        cpu_state = 0;
        was_high = 1;
    } else if (*time_cpu == 0) {
        cpu_state = 1;
        was_high = 0;
    } else {
        cpu_state = was_high ? 0 : 1;
    }

    return cpu_state;
}

// Modbus server
void run_server(int debugFlag) {
    // Declaring local variables
    int STOP_PROGRAM = 0, START_PROGRAM = 0;    // Currently not used - placeholders
    int reConCount = 0, conReqCount = 0, debugTrue = 0;

    // Sets debug flag for debugging messages (ASCII 89 = 'Y')
    if (debugFlag == 89) {
        printf("***DEBUG SELECTED***\n");
        debugTrue = 1;
    }

    // Print to console
    printf("Starting server...\n");

    // Create Modbus context
    modbus_t *ctx = modbus_new_tcp(NULL, SERVER_PORT);
    if (ctx == NULL) {
        fprintf(stderr, "Error: Unable to create Modbus context\n");
        exit(EXIT_FAILURE);
    }

    // Manual Modbus debug only
    /*modbus_set_debug(ctx, TRUE);*/

    // Allocate Modbus mapping
    modbus_mapping_t *mb_mapping = modbus_mapping_new(0, 0, 2, 0); // 1 holding register
    if (mb_mapping == NULL) {
        fprintf(stderr, "Error: Unable to allocate Modbus mapping\n");
        modbus_free(ctx);
        exit(EXIT_FAILURE);
    }

    mb_mapping->tab_registers[0] = 0; // Initialize holding register (CPU_TEMP)
    mb_mapping->tab_registers[1] = 0; // Initialize holding register (START=1, STOP=0)

    // Start listening for connections
    int server_socket = modbus_tcp_listen(ctx, 1); // Backlog of 1 connection
    if (server_socket == -1) {
        fprintf(stderr, "Error: modbus_tcp_listen failed\n");
        modbus_mapping_free(mb_mapping);
        modbus_free(ctx);
        exit(EXIT_FAILURE);
    }

    // Print to console
    printf("Server is listening on port %d\n", SERVER_PORT);

    // Forever while loop
    while (1) {
        // Print to console
        printf("Waiting for a client connection...\n");

        // Accept a client connection
        int client_socket = modbus_tcp_accept(ctx, &server_socket);
        if (client_socket == -1) {
            fprintf(stderr, "Error: modbus_tcp_accept failed: %s\n", modbus_strerror(errno));
            continue; // Retry accepting new connections
        }

        if (reConCount > 10) {
            printf("Number of re-connection attempts has exceeded maximum allowable\n\n");
            exit(EXIT_FAILURE);
        }

        printf("Client connected\n\n");
        reConCount++;

        // DEBUG
        if (debugTrue == 1) {
            printf("\nNumber of connections: %d\n", reConCount);
        }

        // Handle client requests
        while (1) {

            // Defining Variables
            uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
            int rc = modbus_receive(ctx, query);
            uint8_t function_code = query[7];

            if (rc > 0) {
                // DEBUG Switch case dependent on request type
                if (debugTrue == 1) {
                    switch (function_code) {
                        case 0x03:
                            printf("DEBUG: Received Modbus Read Holding Registers request (length: %d)\n", rc);
                        break;
                        case 0x06:
                            printf("DEBUG: Received Modbus Write Single Register request (length: %d)\n", rc);
                        break;
                        case 0x10:
                            printf("DEBUG: Received Modbus Write Multiple Registers request (length: %d)\n", rc);
                        break;
                        default:
                            printf("DEBUG: Received unknown Modbus request (function code: 0x%02X, length: %d)\n", function_code, rc);
                        break;
                    }
                }

                // If a read request has been received
                if (function_code == 0x03) {
                    mb_mapping->tab_registers[0] = get_cpu_temp(debugTrue);
                }

                // Send response to the client
                if (modbus_reply(ctx, query, rc, mb_mapping) == -1) {
                    fprintf(stderr, "Error: modbus_reply failed: %s\n", modbus_strerror(errno));
                    break; // Exit client loop on failure
                }

                // If a read request has been processed
                if (function_code == 0x03 && debugTrue == 1) {
                    printf("DEBUG: Read request processed.\n - Updated Value in the cpu temp Register[0]: %d\n", mb_mapping->tab_registers[0]);
                }

                // If a write request has been processed
                if (function_code == 0x06 && debugTrue == 1) {
                    printf("DEBUG: Write request processed.\n - Updated Value in the start/stop Register[1]: %d\n", mb_mapping->tab_registers[1]);

                    // If stop request received
                    if (mb_mapping->tab_registers[1] != 1) {
                        // STOP_PROGRAM - (fails safe)
                        STOP_PROGRAM = 1;
                        START_PROGRAM = 0;
                        printf("Stop request received. Stopping process...\n");
                    } else {
                        // START_PROGRAM
                        STOP_PROGRAM = 0;
                        START_PROGRAM = 1;
                        printf("Start request received. Starting/Continuing process...\n");
                    }
                }
            }

            // Error handling
            else if (rc == -1) {
                if (errno == ECONNRESET) {
                    // Suppress redundant "Connection reset by peer" errors in application logs
                    printf("Client disconnected gracefully\n");
                } else {
                    // Log unexpected errors
                    fprintf(stderr, "Error: modbus_receive failed (errno: %d): %s\n", errno, modbus_strerror(errno));
                }
                break;
            }

            printf("\nEnd of client request No: %d\n\n", conReqCount);
            conReqCount++;

            if (conReqCount > 200) {
                printf("Number of requests has exceeded maximum allowable. Shutting down server...\n\n");
                exit(EXIT_SUCCESS);
            }
        }

        // Close the client socket only after the client disconnects
        printf("Closing client connection...\n");
        close(client_socket);
    }

    // Cleanup resources (this should never be reached, but kept as back-up)
    printf("Shutting down server...\n");
    close(server_socket);
    modbus_mapping_free(mb_mapping);
    modbus_free(ctx);
    printf("Server shutdown complete\n");
}

// Modbus client
void run_client(int log_rate, int debugFlag) {
    // Sets debug flag for debugging messages (ASCII 89 = 'Y')
    int debugTrue = 0;
    if (debugFlag == 89) {
        printf("***DEBUG SELECTED***\n");
        debugTrue = 1;
    }

    // Declaring function variables
    int stopStart_req = 0, checkFlag = 0;
    int valueOfTime_cpu = 0, tempTarget = 0;
    int *time_cpu = &valueOfTime_cpu;
    int clockTime = log_rate - 1;
    uint16_t read_value[1];

    // Select target CPU value
    printf("Enter max allowable CPU temperature (degrees C): ");
    scanf("%d", &tempTarget);

    // While loop ensures the user enters a valid argument
    int i = 1;
    while (i == 1) {
        if (tempTarget <= 0 || tempTarget > 150) {
            printf("CPU temperature out of range\nPlease enter a value between 0 and 150: ");
            scanf("%d", &tempTarget);
        } else {
            i = 0;
        }
    }

    // Converting to milli degrees C
    tempTarget = tempTarget * 1000;

    // Console comms
    printf("starting client\n");

    // Initialise Modbus TCP context
    modbus_t *ctx = modbus_new_tcp(SERVER_IP, SERVER_PORT);

    // manual modbus debug only
    //modbus_set_debug(ctx, TRUE);
    //modbus_set_slave(ctx, 0);
    //modbus_set_response_timeout(ctx, 5, 0);  // 5 seconds timeout

    // Error handling
    if (ctx == NULL) {
        fprintf(stderr, "Unable to create Modbus context\n");
        exit(EXIT_FAILURE);
    }
    if (modbus_connect(ctx) == -1) {
        fprintf(stderr, "Connection failed\n");
        modbus_free(ctx);
        exit(EXIT_FAILURE);
    }

    // Console comms
    printf("Connected to server\n");

    // Enter infinite while loop
    while (1) {
        // DEBUG
        if (debugTrue == 1) {
            printf("DEBUG: starting 1s delay\n");
        }

        // Simple 1-second clock
        sleep(1);
        clockTime++;

        // DEBUG
        if (debugTrue == 1) {
            printf("DEBUG: end 1s delay client. Clock time: %d\n", clockTime);
        }

        // Read cpu temp at a user-defined time interval
        if (clockTime == log_rate) {
            // Sets flag for write requests
            checkFlag = 1;

            // DEBUG
            if (debugTrue == 1) {
                printf("DEBUG: clock has reached log rate\n\n");
            }

            // Read a value from the server
            if (modbus_read_registers(ctx, CPU_TEMP, 1, read_value) == -1) {
                fprintf(stderr, "Failed to read register: %s\n", modbus_strerror(errno));
            } else {
                printf("Current CPU temperature of server: [%hu]\n", read_value[0]);
            }

            // Call function to assess CPU temp and set associated flags
            if (monitor_cpu_temp(read_value, time_cpu, debugTrue, tempTarget) == 1) {
                stopStart_req = 1;  // TRUE, start request acknowledged
            } else {
                stopStart_req = 0;  // FALSE, stop request acknowledged
            }

            // Reset clock timer
            clockTime = 0;

            // DEBUG
            if (debugTrue == 1) {
                printf("DEBUG: Clock-time activity complete. Continue program.\n");
            }
        }

        // If a start request is received
        if (stopStart_req == 1 && checkFlag == 1) {
            // DEBUG
            if (debugTrue == 1) {
                printf("DEBUG: Start request has been determined. Requesting now...\n");
            }
            // Write a value to the server
            if (modbus_write_register(ctx, START_STOP, stopStart_req) == -1) {
                fprintf(stderr, "Failed to write register\n");
            } else {
                printf("Write to server complete - Start demanded\n\n");
            }

            // DEBUG
            if (debugTrue == 1) {
                printf("Value written to server: %d\n", stopStart_req);
            }

            // Reset flag to prevent constant write requests
            checkFlag = 0;
        }

        // If a stop request is received
        else if (stopStart_req == 0 && checkFlag == 1) {
            // DEBUG
            if (debugTrue == 1) {
                printf("DEBUG: Stop request has been determined. Requesting now...\n");
            }
            // Write a value to the server
            if (modbus_write_register(ctx, START_STOP, stopStart_req) == -1) {
                fprintf(stderr, "Failed to write register\n\n");
            } else {
                printf("Write to server complete - Stop demanded\n\n");
            }

            // DEBUG
            if (debugTrue == 1) {
                printf("Value written to server: %d\n", stopStart_req);
            }

            // Reset flag to prevent constant write requests
            checkFlag = 0;
        }
    }

    // Close Modbus connection - redundant
    modbus_close(ctx);
    modbus_free(ctx);
}


// Main - handles device/function selection
int main(int argc, char *argv[]) {
    // If server option selected, check format is correct
    if (strcmp(argv[1], "server") == 0) {
        // If the user has entered too many arguments then exit
        if (argc != 3) {
            fprintf(stderr, "Usage: %s [server|client] [debug (Y/N)]\n", argv[0]);
            return EXIT_FAILURE;
        }
        // If debug option correctly selected, call server function
        if (strcmp(argv[2], "Y") == 0 || strcmp(argv[2], "N") == 0) {
            int debugFlag = (int)argv[2][0];
            run_server(debugFlag);
        } else {
            fprintf(stderr, "Usage: %s [server|client] [debug (Y/N)]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    // If client option selected, check format is correct
    if (strcmp(argv[1], "client") == 0) {
        // If the user has entered too many arguments then exit
        if (argc != 4) {
            fprintf(stderr, "Usage: %s [server|client] [logging_rate (seconds)] [debug (Y/N)]\n", argv[0]);
            return EXIT_FAILURE;
        }
        // If debug option correctly selected
        if (strcmp(argv[3], "Y") == 0 || strcmp(argv[3], "N") == 0) {
            // Parse logging rate from the second argument
            int logging_rate = atoi(argv[2]);
            if (logging_rate <= 0) {
                fprintf(stderr, "Invalid logging_rate\n");
                return EXIT_FAILURE;
            }
            if (logging_rate < 3) {
                printf("Logging_rate minimum is 3s\n...forced to 3s\n");
                logging_rate = 3;
            }
            if (logging_rate > 60) {
                printf("Logging_rate maximum is 60s\n...forced to 60s\n");
                logging_rate = 60;
            }

            // Parse debug flag & call client function
            int debugFlag = (int)argv[3][0];
            run_client(logging_rate, debugFlag);

        } else {
            fprintf(stderr, "Usage: %s [server|client] [debug (Y/N)]\n", argv[0]);
            return EXIT_FAILURE;
        }

    } else {
        fprintf(stderr, "Invalid mode. Use 'server' or 'client'.\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}