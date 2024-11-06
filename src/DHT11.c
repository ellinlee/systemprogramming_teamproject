#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <wiringPi.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <math.h>

#define MAX_TIME 85 // 안정적으로 데이터를 읽기 위해 타이밍 85로 정의
#define PIN 2          // PIN number for DHT11

// 서버 정보 정의
#define SERVER_ADDRESS "192.168.45.8"
#define SERVER_PORT 8080

// 변수 정의
int data[5] = {0, 0, 0, 0, 0};
float sum_temp = 0.0;
float sum_humidity = 0.0;
int read_times = 0;
int sample_count = 0;

// 함수 선언
void read_data();
void send_average(int sock);
void *server_thread(void *arg);

int main(void)
{
    pthread_t thr_id;

    printf("Temperature and Humidity Check through DHT11 Sensor\n");

    // wiringpu initialize & 실패 시 에러 메시지 출력
    if (wiringPiSetupGpio() == -1)
    {
        perror("WiringPi initialization failed\n");
        return -1;
    }

    // 센서 데이터와 서버 연결을 위한 스레드 생성 & 실패 시 에러 메세지 출력
    if (pthread_create(&thr_id, NULL, server_thread, NULL) != 0)
    {
        perror("Thread creation failed\n");
        return 1;
    }

    // 스레드 종료
    pthread_join(thr_id, NULL);
    return 0;
}

// 서버 연결을 위한 thread 생성
void *server_thread(void *arg)
{
    struct sockaddr_in server_addr;
    int sock;

    // 소켓 생성
    sock = socket(AF_INET, SOCK_STREAM, 0);
    // 실패 시 에러 메세지 출력
    if (sock == -1)
    {
        perror("Socket creation error\n");
        return NULL;
    }

    // server_add 0으로 초기화
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    server_addr.sin_port = htons(SERVER_PORT);

    // 서버에 연결 & 실패 시 에러 메세지 출력
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Server connection failed");
        close(sock);
        return NULL;
    }

    // 성공 시 연결 성공 메세지 출력
    printf("Connected to server\n");

    // 서버로 20초 간격으로 측정 데이터 평균 send
    while (1)
    {
        send_average(sock);
        delay(2000);
    }

    close(sock);
    return NULL;
}

// 센서로부터 데이터 read
void read_data()
{
    int state = HIGH;
    int count = 0;
    int bit_index = 0;

    memset(data, 0, sizeof(data)); // 데이터를 받아 올 array 초기화

    pinMode(PIN, OUTPUT); // set the pin to output mode - 파이의 핀 사용 준비

    // DHT11은 PIN을 LOW로 설정하고 18ms동안 유지시 이를 데이터 요청 신호로 인식
    digitalWrite(PIN, LOW); // 데이터를 받기 위해 pin을 low로 설정
    delay(18);              // state를 low로 18초간 유지

    // PIN을 HIGH로 설정하고 40ms동안 유지시켜 센서가 데이터를 준비할 수 있는 시간을 제공
    digitalWrite(PIN, HIGH); // 핀 high로 설정 - 데이터 요청 신호 종료
    delayMicroseconds(40);   // 센서가 준비될 때까지 40ms 대기
    pinMode(PIN, INPUT);     // 센서로 부터 데이터를 받기 위한 준비가 되었음을 PIN을 input으로 설정

    // 데이터 읽기
    for (int i = 0; i < MAX_TIME; i++)
    {
        count = 0;
        // 각 상태에 대한 count 세기
        while (digitalRead(PIN) == state)
        {
            count++;
            delayMicroseconds(1);
            // 무한루프 탈출 
            if (count == 255)
            {
                break;
            }
        }

        // pin의 현재 상태 저장
        state = digitalRead(PIN);

        // 무한루프 탈출
        if (count == 255)
        {
            break;
        }

        /* 
        처음 4번은 연결상태 확인하는 handshake부분 - 무시
        DHT11 센서의 각 비트는 HIGH, LOW 상태 변화 페어로 전달
        HIGH의 상태 길이는 비트 값을 나타냄(26-28ms:0, 70ms:1)
        => LOW 상태 이후 HIGH 상태 길이 측정하여 비트 값 결정함
        */
        if ((i >= 4) && (i % 2 == 0))
        {
            int bit_pos = (bit_index / 8);       // byte position - 현재 비트가 어느 바이트에 위치하는지
            int bit_shift = 7 - (bit_index % 8); // bit shift position - 바이트 내에서의 비트의 위치.

            // 비트 값 결정 후 바이트와 위치에 저장 , HIGH 상태가 16ms보다 길면 1, 짧으면 0으로 결정
            int bit_value;

            // count에 따라 bit_value 결정
            if (count > 50)
            {
                bit_value = 1; 
            }
            else
            {
                bit_value = 0; 
            }

            // 결정된 비트 값을 바이트 내 올바른 위치로 이동
            int shifted_bit = bit_value << bit_shift;

            // data 배열에서 현재 비트를 저장할 바이트 값 get
            int current_byte = data[bit_pos];

            // 이동된 비트 값을 현재 바이트 값과 결합
            current_byte |= shifted_bit;

            // 결합된 결과 data에 다시 저장
            data[bit_pos] = current_byte;

            // 옆으로 index 이동
            bit_index++;
        }
    }

    // 체크섬 확인 및 데이터 출력
    if ((bit_index >= 40) && (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF)))
    {
        float humidity = (float)data[0] + (float)data[1] * 0.1;
        float temperature = (float)data[2] + (float)data[3] * 0.1;

        // 평균 출력을 위해 humidity,temp. 더하기
        sum_humidity += humidity;
        sum_temp += temperature;

        // valid 한 데이터가 들어온 것에 대한 times 추가
        read_times++;

        printf("Humidity = %.1f%% Temperature = %.1f°C\n", humidity, temperature);
    }
    else
    {
        printf("Invalid data. skip! \n");
    }
}

// 평균 계산 및 data send to server
void send_average(int sock)
{
    char message[100];

    read_data();
    sample_count++;

    // 측정 횟수가 10번이 되되면 메세지 출력
    if (sample_count == 10)
    {
        // valid한 값이 측정되었을 때만 평균 계산
        if (read_times > 0)
        {
            // 평균 계산
            float avg_humidity = sum_humidity / read_times;
            float avg_temp = sum_temp / read_times;

            // 메세지 출력
            printf("Average Temperature and Humidity are calculated!\n");
            printf("Average Temperature = %.1f°C, Average Humidity = %.1f%%\n", avg_temp, avg_humidity);

            // message에 저장
            snprintf(message, sizeof(message), "%.1f %.1f", avg_temp, avg_humidity);

            // 서버에 전달 실패시 메세지 출력
            if (send(sock, message, strlen(message), 0) == -1)
            {
                perror("Transmission failed");
            }
        }
        // valid한 데이터가 센서로부터 얻지 못했을 때 오류 메세지 출력
        else
        {
            printf("No valid data to calculate averages\n");
        }
        // 반복해서 데이터를 읽고자 변수 초기화
        sample_count = 0;
        sum_humidity = 0.0;
        sum_temp = 0.0;
        read_times = 0;
    }
}