#include "ardaos.h"
#include "libc.h"

__attribute__((section(".text.entry")))
void _start(char* args) {
    sys_create_window("ArdaOS Web Tarayici", 450, 200);
    printf("[ BROWSER ] Google sunucusuna baglaniliyor...");

    int socket_fd = sys_open("NET", "TCP");
    
    if (socket_fd >= 0) {
        // HTTP GET İsteği (Artık Google'dan veri istiyoruz)
        char* http_request = "GET / HTTP/1.1\r\nHost: www.google.com\r\nConnection: close\r\n\r\n";
        
        // VFS, bağlantı (SYN-ACK) kurulana kadar boşluk dönecek. Kurulduğunda veriyi fırlatacağız!
        int zaman_asimi = 0;
        int istek_yollandi = 0;
        
        // Çekirdeğin libc'deki malloc ile bize tahsis edeceği devasa yanıt belleği
        char* gelen_html = (char*)malloc(8192); 
        
        while(zaman_asimi < 500000) { 
            // Eğer henüz isteği yollamadıysak yollamayı dene (Bağlantı ESTABLISHED olunca yollar)
            if (istek_yollandi == 0) {
                if (sys_write(socket_fd, (unsigned char*)http_request, strlen(http_request)) > 0) {
                    printf("[ BROWSER ] TCP El Sikismasi Tamam! HTTP GET Istegi Yollandi!");
                    istek_yollandi = 1;
                }
            } else {
                // İstek yollandıysa HTML yanıtını beklemeye başla
                int okunan = sys_read(socket_fd, (unsigned char*)gelen_html, 8192);
                if (okunan > 0) {
                    printf("========== GELEN HTML VERISI ==========");
                    printf(gelen_html);
                    printf("=======================================");
                    break;
                }
            }
            sys_yield();
            zaman_asimi++;
        }
        
        if (zaman_asimi >= 500000) printf("[ HATA ] Sunucu yanit vermedi.");
        
        free(gelen_html);
        sys_close(socket_fd);
    } 
    
    sys_exit();
    while(1);
}