/**
 * @brief   A Linux user space program that communicates with the LKM (linked list version).
 * It writes multiple strings to the LKM and reads them back.
 * For this example to work the device must be called /dev/simple_driver_list.
 *
 * Modified from Derek Molloy (http://www.derekmolloy.ie/ )
 */
 #include <stdio.h>
 #include <stdlib.h>
 #include <errno.h>
 #include <fcntl.h>
 #include <string.h>
 #include <unistd.h>
 
 #define BUFFER_LENGTH 256         ///< The buffer length
 #define NUM_MESSAGES 3            ///< Number of messages to send
 
 int main() {
     int ret, fd;
     char receive[BUFFER_LENGTH];   ///< The receive buffer from the LKM
     char stringToSend[BUFFER_LENGTH];
     int i;
 
     printf("Starting linked list device test code example...\n");
 
     fd = open("/dev/simple_driver_list", O_RDWR); // Open the linked list device
     if (fd < 0) {
         perror("Failed to open the device...");
         return errno;
     }
 
     // Write multiple messages
     for (i = 0; i < NUM_MESSAGES; i++) {
         printf("Type in a short string to send to the kernel module (message %d):\n", i + 1);
         if (fgets(stringToSend, BUFFER_LENGTH, stdin) == NULL) {
             perror("Error reading input");
             close(fd);
             return errno;
         }
         // Remove trailing newline if present
         size_t len = strlen(stringToSend);
         if (len > 0 && stringToSend[len - 1] == '\n') {
             stringToSend[len - 1] = '\0';
         }
         printf("Writing message %d to the device [%s].\n", i + 1, stringToSend);
 
         ret = write(fd, stringToSend, strlen(stringToSend)); // Send the string to the LKM
         if (ret < 0) {
             perror("Failed to write the message to the device.");
             close(fd);
             return errno;
         }
     }
 
     printf("Press ENTER to read back all messages from the device...\n");
     getchar();
 
     printf("Reading messages from the device...\n");
 
     // Read back all the messages
     for (i = 0; i < NUM_MESSAGES; i++) {
         memset(receive, 0, BUFFER_LENGTH); // Clear the receive buffer
         ret = read(fd, receive, BUFFER_LENGTH);       // Read the response from the LKM
         if (ret < 0) {
             perror("Failed to read the message from the device.");
             close(fd);
             return errno;
         }
         if (ret == 0) {
             printf("Reached the end of messages from the device.\n");
             break;
         }
         printf("Received message %d: [%s]\n", i + 1, receive);
     }
 
     printf("End of the program\n");
 
     close(fd);
     return 0;
 }