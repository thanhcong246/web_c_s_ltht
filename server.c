#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


#define PORT 12345
#define BUFFER_SIZE 104857600

const char *get_file_extension(const char *file_name) {
    const char *dot = strrchr(file_name, '.');
    if (!dot || dot == file_name) {
        return "";
    }
    return dot + 1;
}

const char *get_mime_type(const char *file_ext) {
    if (strcasecmp(file_ext, "html") == 0 || strcasecmp(file_ext, "htm") == 0) {
        return "text/html";
    } else if (strcasecmp(file_ext, "txt") == 0) {
        return "text/plain";
    } else if (strcasecmp(file_ext, "jpg") == 0 || strcasecmp(file_ext, "jpeg") == 0) {
        return "image/jpeg";
    } else if (strcasecmp(file_ext, "png") == 0) {
        return "image/png";
    } else {
        return "application/octet-stream";
    }
}

bool case_insensitive_compare(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        if (tolower((unsigned char)*s1) != tolower((unsigned char)*s2)) {
            return false;
        }
        s1++;
        s2++;
    }
    return *s1 == *s2;
}

char *get_file_case_insensitive(const char *file_name) {
    DIR *dir = opendir(".");
    if (dir == NULL) {
        perror("opendir");
        return NULL;
    }

    struct dirent *entry;
    char *found_file_name = NULL;
    while ((entry = readdir(dir)) != NULL) {
        if (case_insensitive_compare(entry->d_name, file_name)) {
            found_file_name = entry->d_name;
            break;
        }
    }

    closedir(dir);
    return found_file_name;
}

char *url_decode(const char *src) {
    size_t src_len = strlen(src);
    char *decoded = malloc(src_len + 1);
    size_t decoded_len = 0;

    // decode %2x to hex
    for (size_t i = 0; i < src_len; i++) {
        if (src[i] == '%' && i + 2 < src_len) {
            int hex_val;
            sscanf(src + i + 1, "%2x", &hex_val);
            decoded[decoded_len++] = hex_val;
            i += 2;
        } else {
            decoded[decoded_len++] = src[i];
        }
    }

    // add null terminator
    decoded[decoded_len] = '\0';
    return decoded;
}

void build_http_response(const char *file_name, 
                        const char *file_ext, 
                        char *response, 
                        size_t *response_len) {
    // Check if the requested file is in the blocklist
    FILE *blocklist = fopen("block.txt", "r");
    if (blocklist != NULL) {
        char line[256];
        while (fgets(line, sizeof(line), blocklist) != NULL) {
            // Remove newline character from the end of the line
            size_t len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') {
                line[len - 1] = '\0';
            }

            // Check if the requested file is in the blocklist
            if (case_insensitive_compare(line, file_name)) {
                // File is blocked, send a response to the client
                snprintf(response, BUFFER_SIZE,
                         "HTTP/1.1 403 Forbidden\r\n"
                         "Content-Type: text/plain\r\n"
                         "\r\n"
                         "403 Forbidden: This URL is blocked. Please go back.");

                *response_len = strlen(response);
                fclose(blocklist);
                return;
            }
        }
        fclose(blocklist);
    }

    // Continue with the rest of the function if the file is not blocked

    // build HTTP header
    const char *mime_type = get_mime_type(file_ext);

    // declare file_size variable
    off_t file_size;

    // obtain file size using fstat
    int file_fd = open(file_name, O_RDONLY);
    if (file_fd == -1) {
        snprintf(response, BUFFER_SIZE,
                 "HTTP/1.1 404 Not Found\r\n"
                 "Content-Type: text/plain\r\n"
                 "\r\n"
                 "404 Not Found");
        *response_len = strlen(response);
        return;
    }

    struct stat file_stat;
    fstat(file_fd, &file_stat);
    file_size = file_stat.st_size;

    char *header = (char *)malloc(BUFFER_SIZE * sizeof(char));
    snprintf(header, BUFFER_SIZE,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "\r\n",
             mime_type, (long)file_size);

    // copy header to response buffer
    *response_len = 0;
    memcpy(response, header, strlen(header));
    *response_len += strlen(header);

    // copy file to response buffer
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, 
                            response + *response_len, 
                            BUFFER_SIZE - *response_len)) > 0) {
        *response_len += bytes_read;
    }

    // free allocated resources
    free(header);
    close(file_fd);
}


void *handle_client(void *arg) {
    int client_fd = *((int *)arg);
    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));

    // receive request data from client and store into buffer
    ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
    if (bytes_received > 0) {
        // check if request is GET
        regex_t regex;
        regcomp(&regex, "^GET /([^ ]*) HTTP/1", REG_EXTENDED);
        regmatch_t matches[2];

        if (regexec(&regex, buffer, 2, matches, 0) == 0) {
            // extract filename from request and decode URL
            buffer[matches[1].rm_eo] = '\0';
            const char *url_encoded_file_name = buffer + matches[1].rm_so;
            char *file_name = url_decode(url_encoded_file_name);

            // get file extension
            char file_ext[32];
            strcpy(file_ext, get_file_extension(file_name));

            // build HTTP response
            char *response = (char *)malloc(BUFFER_SIZE * 2 * sizeof(char));
            size_t response_len;
            build_http_response(file_name, file_ext, response, &response_len);

            // send HTTP response to client
            send(client_fd, response, response_len, 0);

            free(response);
            free(file_name);
        }
        regfree(&regex);
    }
    close(client_fd);
    free(arg);
    free(buffer);
    return NULL;
}


int file_exists(const char* filepath) {
    struct stat buffer;
    return (stat(filepath, &buffer) == 0);
}

void create_html_file(char* title, char* content) {
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "/home/connguyen/Desktop/text_connect/%s.html", title);

    FILE *file = fopen(filepath, "w");
    if (file == NULL) {
        perror("Error creating HTML file");
        exit(EXIT_FAILURE);
    }

    fprintf(file, "%s", content);

    fclose(file);
}

void receive_data(int socket, char* buffer, size_t size) {
    ssize_t bytes_received = recv(socket, buffer, size, 0);
    if (bytes_received < 0) {
        perror("Error receiving data");
        exit(EXIT_FAILURE);
    }
    buffer[bytes_received] = '\0';
}

void list_html_files(int client_socket) {
    char directory_path[1024] = "/home/connguyen/Desktop/text_connect/";
    DIR *dir;
    struct dirent *ent;

    if ((dir = opendir(directory_path)) != NULL) {
        // Loop through all files in the directory
        while ((ent = readdir(dir)) != NULL) {
            if (strstr(ent->d_name, ".html") != NULL) {
                // Send each HTML file name to the client
                send(client_socket, ent->d_name, strlen(ent->d_name), 0);
                send(client_socket, "\n", 1, 0);
            }
        }
        closedir(dir);
    } else {
        perror("Error opening directory");
        exit(EXIT_FAILURE);
    }
}

void delete_html_file(int client_socket, char* title) {
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "/home/connguyen/Desktop/text_connect/%s.html", title);

    if (file_exists(filepath)) {
        if (remove(filepath) == 0) {
            char confirmation[] = "File deleted successfully!";
            send(client_socket, confirmation, strlen(confirmation), 0);
        } else {
            perror("Error deleting file");
            exit(EXIT_FAILURE);
        }
    } else {
        char error_message[] = "Error: File does not exist!";
        send(client_socket, error_message, strlen(error_message), 0);
    }
}

// Function to get the IP address of the server
char* get_server_ip() {
    int sockfd;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Connect to an external server (e.g., Google's public DNS server)
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "8.8.8.8", &(addr.sin_addr));

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    // Get the local address of the connected socket
    if (getsockname(sockfd, (struct sockaddr*)&addr, &addr_len) < 0) {
        perror("Get socket name failed");
        exit(EXIT_FAILURE);
    }

    // Convert the IP address to a string
    char* ip_address = inet_ntoa(addr.sin_addr);

    close(sockfd);

    return ip_address;
}

void block_html_file(int client_socket, char* title) {
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "/home/connguyen/Desktop/text_connect/%s", title);

    // Remove newline character from the title
    title[strcspn(title, "\n")] = '\0';

    // Check if the file exists
    if (file_exists(filepath)) {
        char block_filename[] = "block.txt";
        FILE *block_file;

        // Open block.txt in read mode to check if the file is already blocked
        block_file = fopen(block_filename, "r");
        if (block_file == NULL) {
            perror("Error opening block file");
            exit(EXIT_FAILURE);
        }

        // Check if the file is already blocked
        char line[256];
        while (fgets(line, sizeof(line), block_file) != NULL) {
            // Remove newline character from the line
            line[strcspn(line, "\n")] = '\0';

            if (strcmp(line, title) == 0) {
                // File is already blocked
                fclose(block_file);
                char block_message[] = "File is already blocked!";
                send(client_socket, block_message, strlen(block_message), 0);
                return;
            }
        }

        // Close the file after checking
        fclose(block_file);

        // Open block.txt in append mode to add the file
        block_file = fopen(block_filename, "a");
        if (block_file == NULL) {
            perror("Error opening block file");
            exit(EXIT_FAILURE);
        }

        // Add the file to block.txt
        fprintf(block_file, "%s\n", title);

        fclose(block_file);

        char block_message[] = "File blocked successfully!";
        send(client_socket, block_message, strlen(block_message), 0);
    } else {
        char error_message[] = "Error: File does not exist!";
        send(client_socket, error_message, strlen(error_message), 0);
    }
}

int is_file_blocked(const char* title) {
    FILE* block_file;
    char block_filename[] = "block.txt";
    block_file = fopen(block_filename, "r");
    if (block_file == NULL) {
        perror("Error opening block file");
        exit(EXIT_FAILURE);
    }

    char line[256];
    while (fgets(line, sizeof(line), block_file) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        if (strcmp(line, title) == 0) {
            fclose(block_file);
            return 1; // File is blocked
        }
    }

    fclose(block_file);
    return 0; // File is not blocked
}


void unblock_html_file(int client_socket, char* title) {
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "/home/connguyen/Desktop/text_connect/%s", title);

    // Remove newline character from the title
    title[strcspn(title, "\n")] = '\0';

    // Check if the file exists
    if (file_exists(filepath)) {
        // Check if the file is blocked
        if (is_file_blocked(title)) {
            // Unblock the file
            FILE* block_file;
            char block_filename[] = "block.txt";
            block_file = fopen(block_filename, "r");
            if (block_file == NULL) {
                perror("Error opening block file");
                exit(EXIT_FAILURE);
            }

            // Create a temporary file to store unblocked entries
            char temp_filename[] = "temp_block.txt";
            FILE* temp_file = fopen(temp_filename, "w");
            if (temp_file == NULL) {
                perror("Error creating temp block file");
                fclose(block_file);
                exit(EXIT_FAILURE);
            }

            char line[256];
            while (fgets(line, sizeof(line), block_file) != NULL) {
                line[strcspn(line, "\n")] = '\0';
                if (strcmp(line, title) != 0) {
                    // Write unblocked entries to temp file
                    fprintf(temp_file, "%s\n", line);
                }
            }

            fclose(block_file);
            fclose(temp_file);

            // Replace block.txt with temp_block.txt
            if (rename(temp_filename, block_filename) != 0) {
                perror("Error renaming temp block file");
                exit(EXIT_FAILURE);
            }

            char unblock_message[] = "File unblocked successfully!";
            send(client_socket, unblock_message, strlen(unblock_message), 0);
        } else {
            char unblock_error[] = "Error: File is not blocked!";
            send(client_socket, unblock_error, strlen(unblock_error), 0);
        }
    } else {
        char unblock_error[] = "Error: File does not exist!";
        send(client_socket, unblock_error, strlen(unblock_error), 0);
    }
}


int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[1024] = {0};

    // Tạo socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Thiết lập cấu trúc địa chỉ của server
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Gắn cấu trúc địa chỉ với socket
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Lắng nghe kết nối từ client
    if (listen(server_socket, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d\n", PORT);
    while (1) {
         printf("Waiting for a connection...\n");
        
        // ---
        // client info
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int *client_fd = malloc(sizeof(int));
        
        // accept client connection
        if ((*client_fd = accept(server_socket, 
                                (struct sockaddr *)&client_addr, 
                                &client_addr_len)) < 0) {
            perror("accept failed");
            continue;
        }

        // create a new thread to handle client request
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void *)client_fd);
        pthread_detach(thread_id);
        
        

        // Chấp nhận kết nối từ client
        if ((client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        printf("Connection accepted. Waiting for data...\n");

        // Đọc lựa chọn từ client
        receive_data(client_socket, buffer, sizeof(buffer));

        // Xóa newline character nếu có
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }

        // Xử lý lựa chọn
        if (strcmp(buffer, "1") == 0) {
            // Handle request for basic web page path
            char webpage[] = "/home/connguyen/Desktop/text_connect/"; // Thay đổi đường dẫn thực tế của thư mục chứa file HTML
            send(client_socket, webpage, strlen(webpage), 0);
        } else if (strcmp(buffer, "2") == 0) {
            // Handle request to add HTML file
            char title[256], content[1024];

            // Nhận title và content từ client
            receive_data(client_socket, title, sizeof(title));
            receive_data(client_socket, content, sizeof(content));

            // Tạo file HTML
            create_html_file(title, content);

            // Gửi thông báo xác nhận về cho client
            char confirmation[] = "HTML file created successfully!";
            send(client_socket, confirmation, strlen(confirmation), 0);
        }else if (strcmp(buffer, "3") == 0) {
	    // Handle request to edit HTML file
	    char title[256], content[1024];

	    // Nhận title từ client
	    receive_data(client_socket, title, sizeof(title));

	    // Kiểm tra xem file tồn tại hay không
	    char filepath[1024];
	    snprintf(filepath, sizeof(filepath), "/home/connguyen/Desktop/text_connect/%s.html", title);

	    if (!file_exists(filepath)) {
		// Gửi thông báo lỗi nếu file không tồn tại
		char error_message[] = "Error: File does not exist!";
		send(client_socket, error_message, strlen(error_message), 0);
	    } else {
		// Gửi thông báo cho client rằng file tồn tại và sẵn sàng để sửa
		char confirmation[] = "File exists. Ready for editing.";
		send(client_socket, confirmation, strlen(confirmation), 0);

		// Nhận nội dung mới từ client
		receive_data(client_socket, content, sizeof(content));

		// Ghi nội dung mới vào file
		FILE *file = fopen(filepath, "w");
		if (file == NULL) {
		    perror("Error opening file for editing");
		    exit(EXIT_FAILURE);
		}
		fprintf(file, "%s", content);
		fclose(file);

		// Gửi thông báo xác nhận về cho client
		char edit_confirmation[] = "File edited successfully!";
		send(client_socket, edit_confirmation, strlen(edit_confirmation), 0);
	    }
	} else if (strcmp(buffer, "4") == 0) {
	    // Handle request to list HTML files
	    list_html_files(client_socket);
	} else if (strcmp(buffer, "5") == 0) {
	    // Handle request to delete HTML file
	    char title[256];

	    // Receive title from client
	    receive_data(client_socket, title, sizeof(title));

	    // Delete the HTML file
	    delete_html_file(client_socket, title);
	} else if (strcmp(buffer, "6") == 0) {
	    // Handle request to get server IP
	    char* server_ip = get_server_ip();
	    send(client_socket, server_ip, strlen(server_ip), 0);
	} else if (strcmp(buffer, "7") == 0) {
	    // Handle request to block HTML file
	    char title[256];

	    // Receive title from client
	    receive_data(client_socket, title, sizeof(title));

	    // Block the HTML file
	    block_html_file(client_socket, title);
	} else if (strcmp(buffer, "8") == 0) {
	    // Handle request to unblock HTML file
	    char title[256];

	    // Receive title from client
	    receive_data(client_socket, title, sizeof(title));

	    // Unblock the HTML file
	    unblock_html_file(client_socket, title);
	}

        // Đóng socket của client
        close(client_socket);

        // Reset buffer for the next iteration
        memset(buffer, 0, sizeof(buffer));
    }

    // Đóng socket của server (sẽ không bao giờ đến đoạn này vì vòng lặp vô hạn trước đó)
    close(server_socket);
    return 0;
}
