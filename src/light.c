#include <fcntl.h> 
#include <linux/spi/spidev.h> 
#include <linux/types.h> 
#include <stdint.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <sys/ioctl.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <pthread.h> 

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0])) // 배열의 크기를 계산하는 매크로

static const char *DEVICE = "/dev/spidev0.0"; // SPI 장치 파일 경로
static uint8_t MODE = 0; // SPI 모드 설정
static uint8_t BITS = 8; // 전송 비트 수 설정
static uint32_t CLOCK = 1000000; // SPI 클럭 속도 설정
static uint16_t DELAY = 5; // 전송 딜레이 설정

static const char *SERVER_IP = "192.168.45.8"; // 서버 IP 주소
static const int SERVER_PORT = 8080; // 서버 포트 번호

// SPI 장치를 준비하는 함수
static int prepare(int fd) { 
    if (ioctl(fd, SPI_IOC_WR_MODE, &MODE) == -1) { // SPI 모드 설정
        perror("Can't set MODE");
        return -1;
    }

    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &BITS) == -1) { // 전송 비트 수 설정
        perror("Can't set number of BITS");
        return -1;
    }

    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &CLOCK) == -1) { // SPI 클럭 속도 설정
        perror("Can't set write CLOCK");
        return -1;
    }

    if (ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &CLOCK) == -1) { // SPI 읽기 클럭 속도 설정
        perror("Can't set read CLOCK");
        return -1;
    }

    return 0;
}

// 채널의 차동 제어 비트를 설정하는 함수
uint8_t control_bits_differential(uint8_t channel) {
    return (channel & 7) << 4;
}

// 채널의 제어 비트를 설정하는 함수
uint8_t control_bits(uint8_t channel) {
    return 0x8 | control_bits_differential(channel);
}

// ADC 값을 읽는 함수
int readadc(int fd, uint8_t channel) { 
    uint8_t tx[] = {1, control_bits(channel), 0}; // 전송 버퍼 초기화
    uint8_t rx[3]; // 수신 버퍼

    // SPI 전송 구조체 초기화
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = ARRAY_SIZE(tx),
        .delay_usecs = DELAY,
        .speed_hz = CLOCK,
        .bits_per_word = BITS,
    };

    // SPI 메시지 전송
    if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        perror("IO Error");
        abort();
    }

    // 수신된 데이터 반환
    return ((rx[1] << 8) & 0x300) | (rx[2] & 0xFF);
}

// 통신 쓰레드 함수
void *communication_thread(void *arg) {
    int fd = *((int *)arg); // 전달된 파일 디스크립터 가져오기
    int num_readings = 10; // 데이터 읽기 횟수
    int sum = 0; // 조도 센서 값 누적을 위한 변수

    int sock; // 소켓 파일 디스크립터
    struct sockaddr_in server; // 서버 주소 구조체

    sock = socket(AF_INET, SOCK_STREAM, 0); // 소켓 생성
    if (sock == -1) {
        perror("Could not create socket");
        pthread_exit(NULL); // 실패 시 쓰레드 종료
    }

    server.sin_addr.s_addr = inet_addr(SERVER_IP); // 서버 IP 주소 설정
    server.sin_family = AF_INET; // 주소 체계 설정 (IPv4)
    server.sin_port = htons(SERVER_PORT); // 서버 포트 번호 설정

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) { // 서버에 연결 시도
        perror("Connect failed. Error");
        pthread_exit(NULL); // 실패 시 쓰레드 종료
    }
    printf("Connected to server\n");

    while (1) {
        sum = 0; // 합계 초기화
        num_readings = 10; // 읽기 횟수 초기화

        while (num_readings > 0) { // 읽기 횟수만큼 반복
            int value = readadc(fd, 0); // ADC 값 읽기
            printf("Light sensor value: %d\n", value); // 읽은 값 출력
            sum += value; // 읽은 값을 합계에 추가
            num_readings--; // 읽기 횟수 감소
            usleep(2000000); // 2초 대기
        }

        int average = sum / 10; // 평균 값 계산
        printf("Average Light Sensor Value: %d\n", average); // 평균 값 출력

        char message[20]; // 메시지 버퍼
        snprintf(message, sizeof(message), "%d", average); // 평균 값을 문자열로 변환하여 메시지에 저장
        if (send(sock, message, strlen(message), 0) < 0) { // 서버로 메시지 전송
            perror("Send failed");
            pthread_exit(NULL); // 실패 시 쓰레드 종료
        }
    }

    close(sock); // 소켓 닫기
    pthread_exit(NULL); // 쓰레드 종료
}

int main(int argc, char **argv) {
    int fd = open(DEVICE, O_RDWR); // SPI 장치 열기
    if (fd <= 0) {
        perror("Device open error"); // 열기 실패 시 에러 메시지 출력
        return -1;
    }

    if (prepare(fd) == -1) { // SPI 장치 준비
        perror("Device prepare error"); // 준비 실패 시 에러 메시지 출력
        return -1;
    }

    pthread_t thread_id; // 쓰레드 ID 변수
    pthread_create(&thread_id, NULL, communication_thread, (void *)&fd); // 통신 쓰레드 생성

    pthread_join(thread_id, NULL); // 쓰레드 종료 대기

    close(fd); // SPI 장치 닫기
    return 0; // 프로그램 종료
}
