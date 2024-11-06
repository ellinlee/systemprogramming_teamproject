#include <sys/stat.h> 
#include <sys/types.h> 
#include <fcntl.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <time.h> 
#include <wiringPi.h> 
#include <softPwm.h> 
#include <string.h> 
#include <arpa/inet.h> 
#include <pthread.h>  

#define IN 0 
#define OUT 1
#define LOW 0 
#define HIGH 1 
#define PIR_PIN 20 // PIR 센서 핀 정의
#define POUT 21 // 출력 핀 정의
#define SERVO 18 // 서보모터 핀 정의
#define VALUE_MAX 40 // 최대 값 정의

#define SERVER_IP "192.168.45.8"  // 서버 IP 주소 정의
#define SERVER_PORT 8080 // 서버 포트 정의

static int GPIOExport(int pin) {
    #define BUFFER_MAX 3
    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;

    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open export for writing!\n");
        return(-1);
    }

    bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
    write(fd, buffer, bytes_written);
    close(fd);
    return(0);
}

static int GPIOUnexport(int pin) {
    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;

    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open unexport for writing!\n");
        return(-1);
    }

    bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
    write(fd, buffer, bytes_written);
    close(fd);
    return(0);
}

static int GPIODirection(int pin, int dir) {
    static const char s_directions_str[] = "in\0out";
    #define DIRECTION_MAX 35
    char path[DIRECTION_MAX];
    int fd;

    snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);

    fd = open(path, O_WRONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open gpio direction for writing!\n");
        return(-1);
    }

    if (-1 == write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3)) {
        fprintf(stderr, "Failed to set direction!\n");
        close(fd);
        return(-1);
    }

    close(fd);
    return(0);
}

static int GPIORead(int pin) {
    char path[VALUE_MAX];
    char value_str[3];
    int fd;

    snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_RDONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open gpio value for reading!\n");
        return(-1);
    }

    if (-1 == read(fd, value_str, 3)) {
        fprintf(stderr, "Failed to read value!\n");
        close(fd);
        return(-1);
    }

    close(fd);
    return(atoi(value_str));
}

static int GPIOWrite(int pin, int value) {
    static const char s_values_str[] = "01";
    char path[VALUE_MAX];
    int fd;

    snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_WRONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open gpio value for writing!\n");
        return(-1);
    }

    if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)) {
        fprintf(stderr, "Failed to write value!\n");
        close(fd);
        return(-1);
    }

    close(fd);
    return(0);
}

// 서버로 데이터를 보내는 함수
void send_data_to_server(int sock, int motion_detected) {
    char message[2]; // 메시지 버퍼 정의
    snprintf(message, sizeof(message), "%d", motion_detected); // 모션 감지 상태를 문자열로 변환하여 message에 저장
    if (send(sock, message, strlen(message), 0) == -1) { // 서버로 데이터를 전송
        perror("send failed"); // 전송 실패 시 에러 메시지 출력
    }
}

// 서버에 연결하는 함수
int connect_to_server() {
    struct sockaddr_in servaddr; // 서버의 주소 정보를 저장할 구조체
    int sock; // 소켓 파일 디스크립터

    // 소켓 생성
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) { // 소켓 생성에 실패한 경우
        perror("socket creation failed");
        return -1;
    }

    // 서버 주소 초기화
    servaddr.sin_family = AF_INET; // 주소 체계 설정 (IPv4)
    servaddr.sin_port = htons(SERVER_PORT); // 포트 번호 설정 (네트워크 바이트 순서로 변환)
    servaddr.sin_addr.s_addr = inet_addr(SERVER_IP); // IP 주소 설정

    // 서버에 연결 시도
    while (connect(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) {
        perror("connection with the server failed, retrying..."); // 연결 실패 시 에러 메시지 출력
        close(sock); // 연결 실패 시 소켓을 닫음
        sleep(1);  // 잠시 대기 후 재시도
        sock = socket(AF_INET, SOCK_STREAM, 0); // 새로운 소켓 생성
        if (sock == -1) { // 소켓 생성에 실패한 경우
            perror("socket creation failed");
            return -1;
        }
    }
    printf("Connected to server\n"); // 연결 성공 시 메시지 출력
    return sock; // 소켓 파일 디스크립터 반환
}

// 클라이언트 스레드 함수
void *client_thread(void *arg) {
    int sock = connect_to_server(); // 서버에 연결
    int cnt = 0; // 카운터 변수
    if (sock == -1) { // 연결 실패 시
        return NULL;
    }

    int state; // 현재 상태
    int prev_state = LOW; // 이전 상태
    time_t last_detection_time = 0; // 마지막 감지 시간

    while (1) {
        state = GPIORead(PIR_PIN); // PIR 센서의 상태를 읽음

        if (state == HIGH) { // 모션이 감지된 경우
            send_data_to_server(sock, 1); // 서버에 데이터 전송
        } else {
            send_data_to_server(sock, 0); // 모션이 감지되지 않은 경우 서버에 데이터 전송
        }
        prev_state = state; // 이전 상태 업데이트
        usleep(100000); // 0.1초 대기
    }

    // 소켓 종료
    close(sock);

    return NULL;
}

int main(int argc, char *argv[]) {
    wiringPiSetupGpio(); // GPIO 설정 초기화
    pinMode(SERVO, OUTPUT); // 서보모터 핀을 출력으로 설정
    softPwmCreate(SERVO, 0, 200); // 소프트웨어 PWM 설정

    // GPIO 핀 활성화
    if (-1 == GPIOExport(PIR_PIN) || -1 == GPIOExport(POUT))
        return 1;

    // GPIO 핀 방향 설정
    if (-1 == GPIODirection(PIR_PIN, IN) || -1 == GPIODirection(POUT, OUT))
        return 2;

    // 클라이언트 스레드 생성
    pthread_t client_tid;
    pthread_create(&client_tid, NULL, client_thread, NULL);

    // 메인 스레드는 무한 루프 내에서 LED와 서보 모터를 제어
    time_t last_detection_time = 0; // 마지막 감지 시간을 저장할 변수
    while (1) {
        int state = GPIORead(PIR_PIN); // PIR 센서의 상태를 읽음

        if (state == HIGH) { // 모션이 감지된 경우
            printf("Motion detected in main loop!\n"); // 감지 메시지 출력
            GPIOWrite(POUT, HIGH); // LED 켬
            softPwmWrite(SERVO, 25); // 서보모터 25도로 설정
            delay(300); // 300ms 대기
            softPwmWrite(SERVO, 5); // 서보모터 5도로 설정
            delay(300); // 300ms 대기
            last_detection_time = time(NULL); // 현재 시간을 마지막 감지 시간으로 설정
        }

        // 마지막 모션 감지 후 최소 5초 동안 LED를 켜둠
        if (difftime(time(NULL), last_detection_time) >= 5) {
            printf("No detection in main loop\n"); // 감지 없음 메시지 출력
            GPIOWrite(POUT, LOW); // LED 끔
            softPwmWrite(SERVO, 0); // 서보모터 0도로 설정
        }

        delay(1000); // 1초 대기
    }

    // GPIO 핀 비활성화
    if (-1 == GPIOUnexport(PIR_PIN) || -1 == GPIOUnexport(POUT))
        return 4;

    return 0;
}
