#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 12345
#define SERVER_IP "192.168.44.128" // Replace with the IP of your server

void send_data(int socket, const char* data) {
    send(socket, data, strlen(data), 0);
}

int main() {
    while (1) {
        int client_socket;
        struct sockaddr_in server_addr;
        char choice[2];

        // Tạo socket
        if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        }

        // Thiết lập cấu trúc địa chỉ của server
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
        server_addr.sin_port = htons(PORT);

        // Kết nối đến server
        if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("Connection failed");
            exit(EXIT_FAILURE);
        }

        printf("Connected to the server.\n");

        // Hiển thị menu cho người dùng
        printf("Menu:\n");
        printf("1. Request Web Page\n");
        printf("2. Add HTML File\n");
        printf("3. Edit HTML File\n");
        printf("4. List HTML Files\n");
        printf("5. Delete HTML File\n");
        printf("6. Show IP Server\n");
        printf("7. Block HTML File\n");
        printf("8. Unblock HTML File\n");
        printf("0. Quit\n");
        printf("Enter your choice (0 to quit): ");
        fgets(choice, sizeof(choice), stdin);

        // Xóa newline character nếu có
        size_t len = strlen(choice);
        if (len > 0 && choice[len - 1] == '\n') {
            choice[len - 1] = '\0';
        }

        // Clear the input buffer
        int c;
        while ((c = getchar()) != '\n' && c != EOF);

        // Gửi lựa chọn đến server
        send_data(client_socket, choice);

        if (strcmp(choice, "0") == 0) {
            // Người dùng chọn thoát
            close(client_socket);
            break;
        } else if (strcmp(choice, "1") == 0) {
            // Nhận và hiển thị đường dẫn trang web từ server
            char webpage[1024];
            ssize_t bytes_received = recv(client_socket, webpage, sizeof(webpage), 0);
            if (bytes_received < 0) {
                perror("Error receiving data");
                exit(EXIT_FAILURE);
            }
            webpage[bytes_received] = '\0';
            printf("Web page path: %s\n", webpage);
        } else if (strcmp(choice, "2") == 0) {
            // Thêm file HTML
            char title[256], content[1024];

            printf("Enter HTML title: ");
            fgets(title, sizeof(title), stdin);
            title[strcspn(title, "\n")] = '\0';  // Remove newline character

            printf("Enter HTML content: ");
            fgets(content, sizeof(content), stdin);

            // Send title and content separately
            send_data(client_socket, title);
            send_data(client_socket, content);

            // Nhận thông báo xác nhận từ server
            char confirmation[1024];
            ssize_t bytes_received = recv(client_socket, confirmation, sizeof(confirmation), 0);
            if (bytes_received < 0) {
                perror("Error receiving data");
                exit(EXIT_FAILURE);
            }
            confirmation[bytes_received] = '\0';
            printf("Server response: %s\n", confirmation);
        }else if (strcmp(choice, "3") == 0) {
	    // Edit existing HTML file
	    char title[256], content[1024];

	    printf("Enter HTML title to edit: ");
	    fgets(title, sizeof(title), stdin);
	    title[strcspn(title, "\n")] = '\0';  // Remove newline character

	    // Send title to server
	    send_data(client_socket, title);

	    // Receive confirmation from server
	    char confirmation[1024];
	    ssize_t bytes_received = recv(client_socket, confirmation, sizeof(confirmation), 0);
	    if (bytes_received < 0) {
		perror("Error receiving data");
		exit(EXIT_FAILURE);
	    }
	    confirmation[bytes_received] = '\0';
	    printf("Server response: %s\n", confirmation);

	    // If the file exists, allow editing
	    if (strcmp(confirmation, "File exists. Ready for editing.") == 0) {
		// Enter new HTML content
		printf("Enter new HTML content: ");
		fgets(content, sizeof(content), stdin);

		// Send content to server
		send_data(client_socket, content);

		// Receive edit confirmation from server
		ssize_t edit_bytes_received = recv(client_socket, confirmation, sizeof(confirmation), 0);
		if (edit_bytes_received < 0) {
		    perror("Error receiving data");
		    exit(EXIT_FAILURE);
		}
		confirmation[edit_bytes_received] = '\0';
		printf("Server response: %s\n", confirmation);
	    }
	} else if (strcmp(choice, "4") == 0) {
	    // List HTML files on the server
	    char filename[1024];
	    ssize_t bytes_received;

	    while ((bytes_received = recv(client_socket, filename, sizeof(filename), 0)) > 0) {
		filename[bytes_received] = '\0';
		printf("%s", filename);
	    }
	} else if (strcmp(choice, "5") == 0) {
	    // Delete an HTML file
	    char title[256];

	    printf("Enter HTML title to delete: ");
	    fgets(title, sizeof(title), stdin);
	    title[strcspn(title, "\n")] = '\0';  // Remove newline character

	    // Send title to server
	    send_data(client_socket, title);

	    // Receive confirmation from server
	    char confirmation[1024];
	    ssize_t bytes_received = recv(client_socket, confirmation, sizeof(confirmation), 0);
	    if (bytes_received < 0) {
		perror("Error receiving data");
		exit(EXIT_FAILURE);
	    }
	    confirmation[bytes_received] = '\0';
	    printf("Server response: %s\n", confirmation);
	} else if (strcmp(choice, "6") == 0) {
	    // Request server IP address
	    send_data(client_socket, "6");

	    // Receive and display server IP from server
	    char server_ip[16];  // Assuming IPv4 address
	    ssize_t ip_bytes_received = recv(client_socket, server_ip, sizeof(server_ip), 0);
	    if (ip_bytes_received < 0) {
		perror("Error receiving server IP");
		exit(EXIT_FAILURE);
	    }
	    server_ip[ip_bytes_received] = '\0';
	    printf("Server IP: %s\n", server_ip);
	} else if (strcmp(choice, "7") == 0) {
	    // Request to block a file
	    char title[256];

	    // Nhập tên file từ người dùng
	    printf("Enter HTML title to block: ");
	    fgets(title, sizeof(title), stdin);
	    title[strcspn(title, "\n")] = '\0';  // Remove newline character

	    // Send title to server
	    send_data(client_socket, title);

	    // Receive confirmation from server
	    char block_message[1024];
	    ssize_t bytes_received = recv(client_socket, block_message, sizeof(block_message), 0);
	    if (bytes_received < 0) {
		perror("Error receiving data");
		exit(EXIT_FAILURE);
	    }
	    block_message[bytes_received] = '\0';
	    printf("Server response: %s\n", block_message);
	} else if (strcmp(choice, "8") == 0) {
	    // Request to unblock HTML file
	    char title[256];

	    // Receive title from user
	    printf("Enter HTML title to unblock: ");
	    fgets(title, sizeof(title), stdin);
	    title[strcspn(title, "\n")] = '\0';  // Remove newline character

	    // Send title to server
	    send_data(client_socket, title);

	    // Receive confirmation from server
	    char unblock_message[1024];
	    ssize_t bytes_received = recv(client_socket, unblock_message, sizeof(unblock_message), 0);
	    if (bytes_received < 0) {
		perror("Error receiving data");
		exit(EXIT_FAILURE);
	    }
	    unblock_message[bytes_received] = '\0';
	    printf("Server response: %s\n", unblock_message);
	}
	else {
            printf("Invalid choice. Please try again.\n");
        }

        // Đóng socket
        close(client_socket);
    }

    return 0;
}
