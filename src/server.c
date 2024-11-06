#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <math.h>
#include <wiringPi.h>
#include <softTone.h>

// GPIO 관련 설정
#define POUT 18 // 부저 핀 번호
#define POUT1 20 // LED 핀 번호
#define LOW 0 // LOW 값 정의
#define HIGH 1 // HIGH 값 정의
#define IN 0 // 입력 방향 정의
#define OUT 1 // 출력 방향 정의
#define BUFFER_SIZE 1024 // 버퍼 크기 정의
#define BUFFER_MAX 3 // 버퍼 최대 크기 정의
#define VALUE_MAX 40 // 값의 최대 크기 정의
#define DIRECTION_MAX 35 // 방향 문자열 최대 크기 정의

// port 번호 설정
#define SERVER_PORT 8080 // 서버 포트 번호 정의

// 클라이언트 IP 주소 설정
#define TEMP_CLIENT_IP "192.168.45.11" // 온도 클라이언트 IP 주소
#define LIGHT_CLIENT_IP "192.168.45.10" // 조도 클라이언트 IP 주소
#define PIR_CLIENT_IP "192.168.45.4" // PIR 클라이언트 IP 주소

// WBGT 임계치 설정
#define WBGT_LIMIT 15 // WBGT 임계치 정의

// 전역 변수
float temperature = 0.0, humidity = 0.0, tg = 0.0, wbgt = 0.0, hum_temperature = 0.0; // 온도, 습도, 흑구온도, WBGT, 습구온도 변수
int temp_flag = 0, light_flag = 0, enable = 0; // 수신 상태 플래그 변수
const int minFreq = 200; // 주파수의 최소값 설정
const int maxFreq = 1000; // 주파수의 최대값 설정
const int step = 10; // 주파수를 증가/감소시키는 단위 설정
const int delayTime = 10; // 딜레이 시간 설정 (밀리초 단위, 즉 0.01초)

// 함수 선언
static int GPIOExport(int pin); // GPIO 핀을 활성화하는 함수
static int GPIODirection(int pin, int dir); // GPIO 핀의 방향을 설정하는 함수
static int GPIOWrite(int pin, int value); // GPIO 핀에 값을 쓰는 함수
static int GPIOUnexport(int pin); // GPIO 핀을 비활성화하는 함수

void* handle_client_temp(void* arg); // 온도 클라이언트 요청을 처리하는 함수
void* handle_client_light(void* arg); // 조도 클라이언트 요청을 처리하는 함수
void* handle_client_PIR(void* arg); // PIR 클라이언트 요청을 처리하는 함수
void* alert(void* arg); // 알람 기능을 수행하는 함수
void cal_wbgt(); // WBGT를 계산하고 알람을 활성화하는 함수
void* handle_client(void* arg); // 클라이언트 요청을 처리하는 함수

// 메인 함수
int main()
{
    // GPIO 초기화
    if (GPIOExport(POUT) == -1)
    {
        return 1;
    }

    // GPIO 방향 설정 (출력으로 설정)
    if (GPIODirection(POUT, 1) == -1)
    {
        return 2;
    }

    // POUT1 GPIO 초기화 및 방향 설정
    if (GPIOExport(POUT1) == -1)
    {
        return 1;
    }
    if (GPIODirection(POUT1, 1) == -1)
    {
        return 2;
    }

    int server_sock; // 서버 소켓 파일 디스크립터
    struct sockaddr_in server_addr; // 서버 주소 구조체
    struct sockaddr_in client_addr; // 클라이언트 주소 구조체
    socklen_t client_addr_size; // 클라이언트 주소 크기 변수

    // 소켓 생성
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1)
    {
        perror("socket creation failed"); // 소켓 생성 실패 시 에러 출력
        exit(EXIT_FAILURE); // 프로그램 종료
    }

    // 서버 주소 초기화
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; // IPv4 설정
    server_addr.sin_port = htons(SERVER_PORT); // 포트 번호 설정
    server_addr.sin_addr.s_addr = INADDR_ANY; // 모든 인터페이스에서 수신

    // 소켓 바인딩
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("socket bind failed"); // 소켓 바인딩 실패 시 에러 출력
        close(server_sock); // 소켓 닫기
        exit(EXIT_FAILURE); // 프로그램 종료
    }

    // 연결 대기
    if (listen(server_sock, 5) == -1)
    {
        perror("listen failed"); // 연결 대기 실패 시 에러 출력
        close(server_sock); // 소켓 닫기
        exit(EXIT_FAILURE); // 프로그램 종료
    }
    printf("Server is listening on port %d\n", SERVER_PORT); // 서버가 포트에서 대기 중임을 출력

    while (1)
    {
        client_addr_size = sizeof(client_addr); // 클라이언트 주소 크기 설정
        int* client_sock = malloc(sizeof(int)); // 클라이언트 소켓 파일 디스크립터 동적 할당
        *client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_size); // 클라이언트 연결 수락

        if (*client_sock == -1)
        {
            perror("server accept failed"); // 연결 수락 실패 시 에러 출력
            free(client_sock); // 메모리 해제
            continue; // 다음 반복으로 넘어감
        }

        pthread_t tid; // 쓰레드 ID 변수
        if (pthread_create(&tid, NULL, handle_client, client_sock) != 0)
        {
            perror("pthread_create failed"); // 쓰레드 생성 실패 시 에러 출력
            free(client_sock); // 메모리 해제
        }
        pthread_detach(tid); // 쓰레드를 분리하여 독립적으로 실행되도록 설정
    }

    // 소켓 종료
    close(server_sock);

    // GPIO 해제
    GPIOUnexport(POUT);
    GPIOUnexport(POUT1);

    return 0;
}

// 클라이언트 요청을 처리하는 함수
void* handle_client(void* arg)
{
    int client_sock = *(int*)arg; // 클라이언트 소켓 파일 디스크립터 가져오기
    struct sockaddr_in client_addr; // 클라이언트 주소 구조체
    socklen_t client_addr_len = sizeof(client_addr); // 클라이언트 주소 길이 변수
    getpeername(client_sock, (struct sockaddr*)&client_addr, &client_addr_len); // 클라이언트 주소 정보 가져오기

    char client_ip[INET_ADDRSTRLEN]; // 클라이언트 IP 주소 문자열
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN); // 클라이언트 IP 주소를 문자열로 변환

    if (strcmp(client_ip, PIR_CLIENT_IP) == 0)
    {
        printf("PIR client connected: %s\n", client_ip); // PIR 클라이언트 연결 메시지 출력
        handle_client_PIR(arg); // PIR 클라이언트 처리 함수 호출
    }
    else if (strcmp(client_ip, TEMP_CLIENT_IP) == 0)
    {
        printf("Temperature client connected: %s\n", client_ip); // 온도 클라이언트 연결 메시지 출력
        handle_client_temp(arg); // 온도 클라이언트 처리 함수 호출
    }
    else if (strcmp(client_ip, LIGHT_CLIENT_IP) == 0)
    {
        printf("Light client connected: %s\n", client_ip); // 조도 클라이언트 연결 메시지 출력
        handle_client_light(arg); // 조도 클라이언트 처리 함수 호출
    }
    return NULL;
}

// 온도 클라이언트 요청을 처리하는 함수
void* handle_client_temp(void* arg)
{
    int client_sock = *(int*)arg; // 클라이언트 소켓 파일 디스크립터 가져오기
    char buffer[BUFFER_SIZE]; // 수신 버퍼
    int bytes_received; // 수신된 바이트 수

    while (1)
    {
        bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0); // 데이터 수신
        if (bytes_received <= 0)
        {
            if (bytes_received == 0)
            {
                printf("Temperature client disconnected\n"); // 클라이언트가 연결 종료 시 메시지 출력
            }
            else
            {
                perror("recv failed"); // 수신 실패 시 에러 출력
            }
            break; // 루프 종료
        }

        buffer[bytes_received] = '\0'; // 문자열 종료 문자 추가

        // 문자열에서 온도와 습도를 파싱
        if (sscanf(buffer, "%f %f", &temperature, &humidity) == 2)
        {
            printf("[Parsed Temperature: %.1f, Humidity: %.1f]\n", temperature, humidity); // 파싱된 온도와 습도 출력
            hum_temperature = temperature * atan(0.152 * sqrt(humidity + 8.3136)) + atan(temperature + humidity) - atan(humidity - 1.67633) + 0.00391838 * pow(humidity, 1.5) * atan(0.0231 * humidity) - 4.686;
            humidity = hum_temperature; // 습구 온도 계산

            temp_flag = 1; // 온도 데이터 수신 완료 플래그

            // 온습도 데이터를 받은 후 조도 데이터 수신 상태 확인 후 WBGT 처리
            cal_wbgt();
        }
        else
        {
            printf("Failed to parse temperature and humidity\n"); // 파싱 실패 시 메시지 출력
        }
    }

    close(client_sock); // 클라이언트 소켓 닫기
    free(arg); // 메모리 해제
    return NULL;
}

// 조도 클라이언트 요청을 처리하는 함수
void* handle_client_light(void* arg)
{
    int client_sock = *(int*)arg; // 클라이언트 소켓 파일 디스크립터 가져오기
    char buffer[BUFFER_SIZE]; // 수신 버퍼
    int bytes_received; // 수신된 바이트 수

    while (1)
    {
        bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0); // 데이터 수신
        if (bytes_received <= 0)
        {
            if (bytes_received == 0)
            {
                printf("Light client disconnected\n"); // 클라이언트가 연결 종료 시 메시지 출력
            }
            else
            {
                perror("recv failed"); // 수신 실패 시 에러 출력
            }
            break; // 루프 종료
        }

        buffer[bytes_received] = '\0'; // 문자열 종료 문자 추가

        // 데이터를 파싱하여 조도 추출
        int light;
        if (sscanf(buffer, "%d", &light) == 1)
        {
            printf("[Light intensity: %d]\n", light); // 파싱된 조도 값 출력
            tg = temperature + (0.02 * light) / 100.0; // 흑구온도 계산
            light_flag = 1; // 조도 데이터 수신 완료 플래그

            // 조도 데이터를 받은 후 온습도 데이터 수신 상태 확인 후 WBGT 처리
            cal_wbgt();
        }
        else
        {
            printf("Failed to parse light data\n"); // 파싱 실패 시 메시지 출력
        }
    }

    close(client_sock); // 클라이언트 소켓 닫기
    free(arg); // 메모리 해제
    return NULL;
}

// PIR 클라이언트 요청을 처리하는 함수
void* handle_client_PIR(void* arg)
{
    int client_sock = *(int*)arg; // 클라이언트 소켓 파일 디스크립터 가져오기
    char buffer[BUFFER_SIZE]; // 수신 버퍼
    int bytes_received; // 수신된 바이트 수

    while (1)
    {
        bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0); // 데이터 수신
        if (bytes_received <= 0)
        {
            if (bytes_received == 0)
            {
                printf("PIR client disconnected\n"); // 클라이언트가 연결 종료 시 메시지 출력
            }
            else
            {
                perror("recv failed"); // 수신 실패 시 에러 출력
            }
            break; // 루프 종료
        }

        buffer[bytes_received] = '\0'; // 문자열 종료 문자 추가

        // 데이터를 파싱하여 PIR 데이터 추출
        int pir;
        if (sscanf(buffer, "%d", &pir) == 1)
        {
            if (pir == 1)
            {
                enable = 0; // 모션 감지 시 알람 종료
            }
        }
        else
        {
            printf("Failed to parse PIR data\n"); // 파싱 실패 시 메시지 출력
        }
    }

    close(client_sock); // 클라이언트 소켓 닫기
    free(arg); // 메모리 해제
    return NULL;
}

// 알람 기능을 수행하는 함수
void* alert(void* arg)
{
    int elapsed_time = 0; // 경과 시간 초기화
    wiringPiSetupGpio(); // WiringPi GPIO 초기화
    softToneCreate(POUT); // 소프트 톤 생성

    // 알람 시작 전에 LED 켜기
    if (GPIOWrite(POUT1, HIGH) == -1)
    {
        fprintf(stderr, "Failed to write GPIO value for light!\n");
        return NULL;
    }

    while (elapsed_time < 4000 || enable)
    { // 4초가 지나지 않았거나, enable이 1일 때 루프 실행
        // 주파수를 증가시키며 사이렌 효과
        for (int freq = minFreq; freq <= maxFreq && (elapsed_time < 4000 || enable); freq += step)
        {
            softToneWrite(POUT, freq); // 주파수 설정
            delay(delayTime); // 딜레이
            elapsed_time += delayTime; // 경과 시간 증가
        }

        // 주파수를 감소시키며 사이렌 효과
        for (int freq = maxFreq; freq >= minFreq && (elapsed_time < 4000 || enable); freq -= step)
        {
            softToneWrite(POUT, freq); // 주파수 설정
            delay(delayTime); // 딜레이
            elapsed_time += delayTime; // 경과 시간 증가
        }
    }

    softToneWrite(POUT, 0); // 소리 끄기

    // 알람 종료 후 LED 끄기
    if (GPIOWrite(POUT1, LOW) == -1)
    {
        fprintf(stderr, "Failed to reset GPIO value for light!\n");
    }

    return NULL;
}

// WBGT를 계산하고 알람을 활성화하는 함수
void cal_wbgt()
{
    if (temp_flag && light_flag)
    {
        // WBGT 계산
        printf("Temperature: %.1f, Humidity: %.1f, Tg: %.1f\n", temperature, humidity, tg);
        wbgt = 0.7 * humidity + 0.2 * temperature + 0.1 * tg; // WBGT 계산
        printf("Calculated WBGT: %.1f\n", wbgt); // 계산된 WBGT 출력

        // WBGT 값이 임계치를 초과할 경우 알람을 울림
        if (wbgt >= WBGT_LIMIT)
        {
            printf("WBGT %.1f exceeds the threshold, triggering alarm\n", wbgt);

            enable = 1; // 알람 활성화

            pthread_t alert_thread; // 알람 쓰레드
            if (pthread_create(&alert_thread, NULL, alert, NULL) != 0)
            {
                perror("pthread_create failed"); // 쓰레드 생성 실패 시 에러 출력
                return;
            }

            pthread_detach(alert_thread); // 독립적으로 실행되도록 분리

            // 플래그 리셋
            temp_flag = 0;
            light_flag = 0;
        }
    }
}
// GPIO 제어 함수
static int GPIOExport(int pin)
{
    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;

    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd == -1)
    {
        fprintf(stderr, "Failed to open export for writing!\n");
        return -1;
    }

    bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
    write(fd, buffer, bytes_written);
    close(fd);
    return 0;
}

static int GPIOUnexport(int pin)
{
    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;

    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd == -1)
    {
        fprintf(stderr, "Failed to open unexport for writing!\n");
        return -1;
    }
    bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
    write(fd, buffer, bytes_written);
    close(fd);
    return 0;
}

static int GPIODirection(int pin, int dir)
{
    static const char s_directions_str[] = "in\0out";
    char path[DIRECTION_MAX];
    int fd;

    snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);

    fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        fprintf(stderr, "Failed to open gpio direction for writing!\n");
        return -1;
    }

    if (write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3) == -1)
    {
        fprintf(stderr, "Failed to set direction!\n");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static int GPIORead(int pin)
{
    char path[VALUE_MAX];
    char value_str[3];
    int fd;

    snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_RDONLY);
    if (fd == -1)
    {
        fprintf(stderr, "Failed to open gpio value for reading!\n");
        return -1;
    }

    if (read(fd, value_str, 3) == -1)
    {
        fprintf(stderr, "Failed to read value!\n");
        close(fd);
        return -1;
    }

    close(fd);
    return atoi(value_str);
}

static int GPIOWrite(int pin, int value)
{
    static const char s_values_str[] = "01";
    char path[VALUE_MAX];
    int fd;

    snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        fprintf(stderr, "Failed to open gpio value for writing!\n");
        return -1;
    }

    if (write(fd, &s_values_str[LOW == value ? 0 : 1], 1) != 1)
    {
        fprintf(stderr, "Failed to write value!\n");
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}
