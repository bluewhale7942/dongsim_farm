/* An Example Code Template for ECHO ZONE ESP32-S3 Touch 5" that fully work with CH422G expansion

 * @file Dongsim_Farm_ESP32_S3_Web_01"
 * @author 푸른고래 [Blue Whale]
 * @date 2026-04-21
 * @copyright Copyright (c) 2026 푸른고래 [Blue Whale]
 * ---------------------------------------------------
 * Hardware: ESP32-S3 N16R8 42pin
 * IDE: Arduino IDE 2.3.8
 * ESP32 Core: 3.2.0
 * PSRAM: OPI PSRAM
 * Flash Szie: 16 MB
 * Partition scheme: 16 MB (3MB APP/9.9MB FATFS)
-------------------------------------------------------------
*/

#include <WiFi.h>          // WiFi 연결을 위한 기본 라이브러리
#include <WebServer.h>     // 웹 페이지를 생성하고 접속을 처리하는 라이브러리
#include <ESPmDNS.h>       // IP 대신 'dongsim.local' 같은 주소로 접속하게 해주는 라이브러리
#include <time.h>          // 인터넷망을 통해 정확한 현재 시간을 가져오는 라이브러리
#include <Preferences.h>   // WiFi 비번이나 릴레이 상태를 전원이 꺼져도 기억하게 하는 라이브러리

// --- [전역 변수 설정] ---
bool wifiConnected = false;  // WiFi 연결 성공 여부 확인용
bool mBits[16] = {0};        // 16개 릴레이의 On/Off 상태를 저장하는 배열
int dwValues[20] = {0};      // 설정값(온도 보정 등)을 저장하기 위한 공간

// [S3 전용 핀 맵] 사용 중인 보드의 실제 회로에 맞춰 핀 번호를 수정하세요.
const int relayPins[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 18, 21};

WebServer server(80);        // 80번 포트로 웹 서버 시작
Preferences prefs;           // 비휘발성 메모리 관리 객체

// [NTP 설정] 인터넷 시간 동기화 설정 (한국 표준시 GMT+9)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 9 * 3600;
const int daylightOffset_sec = 0;

// [웹 로그인 설정] 관리자 페이지 접속용 ID/PW
const char* www_username = "admin";
const char* www_password = "1234";

// [릴레이 이름] 웹 화면에 표시될 버튼의 이름들
const char* relayNames[] = {
  "1중좌측열림","1중우측열림","1중좌측닫힘","1중우측닫힘",
  "2중좌측열림","2중우측열림","2중좌측닫힘","2중우측닫힘",
  "천장암막열림","천장암막닫힘","유동팬","배기팬",
  "조명등","관수펌프","예비 1","예비 2"
};

// --- [함수 원형 선언] ---
void handleRoot(); void handleSetup(); void handleSettings();
void controlRelay(int ch, bool state); String getDateTimeString();

// --- [CSS 디자인] 웹 화면의 스타일(유리 효과 디자인) ---
const char commonCSS[] PROGMEM = R"rawliteral(
:root{--glass:rgba(255,255,255,0.045);--glass2:rgba(255,255,255,0.07);--border:rgba(255,255,255,0.08);--text:rgba(255,255,255,0.88);--muted:rgba(255,255,255,0.55);}
*{box-sizing:border-box;}
body{margin:0;font-family:Inter,sans-serif;color:var(--text);background:radial-gradient(900px 600px at 20% 10%,rgba(120,120,255,0.12),transparent 60%),linear-gradient(135deg,#0b1020,#0e1628);min-height:100vh;}
.wrap{display:flex;flex-direction:column;align-items:center;padding:24px;}
.card{width:min(800px,100%);display:grid;grid-template-columns:1fr;gap:16px;padding:18px;border-radius:24px;background:linear-gradient(180deg,var(--glass2),var(--glass));border:1px solid var(--border);backdrop-filter:blur(10px);margin:10px auto;overflow:hidden;}
@media (min-width:768px){.card{grid-template-columns:1fr 1fr;}}
.card.full{grid-template-columns:1fr !important;}
.panel{padding:18px;border-radius:18px;background:rgba(255,255,255,0.04);border:1px solid var(--border);}
.title{font-size:1.1rem;color:var(--muted);margin-bottom:5px;}
.kv{display:flex;justify-content:space-between;padding:10px;margin-top:8px;border-radius:12px;background:rgba(255,255,255,0.03);border:1px solid var(--border);}
input,select{width:100%;height:48px;padding:10px;margin-bottom:10px;border-radius:8px;border:1px solid var(--border);background:#1f2937;color:white;}
button{width:100%;padding:15px;border-radius:10px;border:none;background:#2b344a;color:#e6e9ff;cursor:pointer;font-weight:bold;transition: all 0.2s ease;outline: none;}
button:active{background: #1e2536;transform: scale(0.96);border-radius: 12px;background: #3a4766;transform: scale(0.98);}
.relay-grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:10px;}
.on{background:#2196F3 !important;box-shadow:0 0 15px rgba(33,150,243,0.4);}
.farm-name{display: block; width: 100%; text-align: center; font-size: 1.6rem; font-weight: 800; color: #ffffff; padding: 5px 0; justify-self: center;}
.btn-sm{width:auto;padding:12px 18px;font-size:0.85rem;background:var(--glass2);border:1px solid var(--border);margin: 8px 5px;display: inline-block;}
.footer{margin-top:20px;font-size:0.9rem;text-align:center;}
.footer a {color: #ffffff !important;text-decoration: none;opacity: 0.7;}

/* 마우스를 올렸을 때 효과 (선택 사항) */
.footer a:hover {
  opacity: 1;
  text-decoration: underline;
}
)rawliteral";

// --- [핵심 함수 구현] ---

// 릴레이를 실제로 켜고 끄며, 그 상태를 메모리에 저장하는 함수
void controlRelay(int ch, bool state) {
  if (ch < 0 || ch >= 16) return;
  mBits[ch] = state; // 상태 업데이트
  digitalWrite(relayPins[ch], state ? HIGH : LOW); // 실제 핀 제어
  
  prefs.begin("relay_stat", false); // 상태 저장 시작
  char key[5]; sprintf(key, "r%d", ch);
  prefs.putBool(key, state);
  prefs.end();
}

// 인터넷에서 받아온 현재 시간을 읽기 좋은 텍스트로 변환하는 함수
String getDateTimeString(){
  struct tm t; if(!getLocalTime(&t)) return "--";
  const char* weekDays[] = {"일","월","화","수","목","금","토"};
  char buf[64]; sprintf(buf, "%04d-%02d-%02d (%s) %02d:%02d:%02d", 
                  t.tm_year+1900, t.tm_mon+1, t.tm_mday, weekDays[t.tm_wday], t.tm_hour, t.tm_min, t.tm_sec);
  return String(buf);
}

// [메인 화면 핸들러] 사용자가 처음 접속했을 때 보여주는 페이지
void handleRoot() {
  if (WiFi.status() != WL_CONNECTED) { handleSetup(); return; } // WiFi 안되어 있으면 설정창으로
  if (!server.authenticate(www_username, www_password)) return server.requestAuthentication(); // 로그인 체크
  
  String page = "<html><head><title>동심팜 환경제어 시스템</title><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><style>" + String(FPSTR(commonCSS)) + "</style><script src='https://cdn.jsdelivr.net/npm/chart.js'></script></head><body><div class='wrap'>";
  page += "<div class='card full'><span class='farm-name'>동심팜 (DONGSIM FARM)</span></div>";
  page += "<div class='top-bar'><div id='dt' style='font-size:1.2rem; color:var(--muted); text-align:center; width:100%;'></div></div>";
  page += "<div class='card'><div class='panel'><div class='title'>센서 데이터</div><div class='kv'><span>내부온도</span><span id='temp'>-</span></div><div class='kv'><span>내부습도</span><span id='humi'>-</span></div></div>";
  page += "<div class='panel'><div class='title'>시스템 정보</div><div class='kv'><span>WiFi 신호</span><span id='rssi'>-</span></div><div class='kv'><span>Heap</span><span id='heap'>-</span></div></div></div>";
  page += "<div class='card full'><div style='height:250px;'><canvas id='trendChart'></canvas></div></div>";

  // 릴레이 판넬을 만드는 반복 함수
  auto makePanel = [&](String title, int start, int end) {
    page += "<div class='panel'><div class='title'>" + title + "</div><div class='relay-grid'>";
    for(int i = start; i < end; i++) page += "<button id='btn" + String(i) + "' onclick='relay(" + String(i) + ")' class='" + (mBits[i]?"on":"") + "'>" + String(relayNames[i]) + "</button>";
    page += "</div></div>";
  };

  page += "<div class='card'>"; makePanel("1중창(외측) 제어", 0, 4); makePanel("2중창(내측) 제어", 4, 8); page += "</div>";
  page += "<div class='card'>"; makePanel("암막커튼 / 환경 제어", 8, 12); makePanel("조명등 / 관수 펌프", 12, 16); page += "</div>";
  page += "<div class='card'><button id='fsBtn' class='btn-sm' onclick='toggleFS()'>풀화면 보기</button><button class='btn-sm' onclick=\"location.href='/settings'\">환경설정</button></div>";
  page += "<div class='footer'><a href='https://iotkorea.news' target='_blank'>누구나 쉽게 만드는 하우스 자동화 (DONGSIM FARM)</a></div></div>";

  // 브라우저에서 실행되는 자바스크립트 (실시간 차트 및 업데이트 로직)
  page += R"rawliteral(<script>
  let trendChart;
  let chartUpdateCounter = 0;
  function toggleFS(){ if(!document.fullscreenElement) document.documentElement.requestFullscreen(); else document.exitFullscreen(); }
  function initChart(){
    trendChart = new Chart(document.getElementById('trendChart'),{
      type:'line', 
      data:{
        labels:[], 
        datasets:[
          {label:'온도(°C)', borderColor:'#ff5252', backgroundColor:'rgba(255,82,82,0.1)', data:[], yAxisID:'y', tension:0.3, fill:true},
          {label:'습도(%)', borderColor:'#2196f3', backgroundColor:'rgba(33,150,243,0.1)', data:[], yAxisID:'y1', tension:0.3, fill:true}
        ]
      },
      options:{ 
        responsive:true, 
        maintainAspectRatio:false, 
        animation:false,
        scales:{ 
          // 왼쪽 온도 축 설정
          y:{
            type:'linear',
            position:'left',
            min:0, max:50,
            title:{ display:true, text:'온도(°C)', color:'#ff5252' }, // 축 제목 색상
            ticks:{ color:'#ff5252' }, // 눈금 숫자 색상
            grid:{ color:'rgba(255,82,82,0.1)' } // 가로 그리드 선 살짝 붉게 (선택사항)
          }, 
          // 오른쪽 습도 축 설정
          y1:{
            type:'linear',
            position:'right',
            min:0, max:100,
            grid:{ drawOnChartArea:false }, // 습도 축 그리드는 온도 축과 겹치지 않게 가림
            title:{ display:true, text:'습도(%)', color:'#2196f3' }, // 축 제목 색상
            ticks:{ color:'#2196f3' } // 눈금 숫자 색상
          } 
        },
        plugins: {
          legend: {
            labels: { color: '#ffffff' } // 상단 범례 글자색 (흰색 유지)
          }
        }
      }
    });
  }
  async function update(){ // 주기적으로 서버에서 데이터를 가져오는 함수
    try {
      let res = await (await fetch('/all_data')).json();
      let t = res.temp, h = res.humi;
      document.getElementById('temp').innerText = t + " °C";
      document.getElementById('humi').innerText = h + " %";
      document.getElementById('dt').innerText = res.datetime;
      document.getElementById('rssi').innerText = res.rssi;
      document.getElementById('heap').innerText = res.heap + " KB";
      for(let i=0; i<16; i++){
        let b = document.getElementById('btn'+i);
        if(res.m[i] == 1) b.classList.add('on'); else b.classList.remove('on');
      }
      if(++chartUpdateCounter >= 5) { // 10초(2초*5회)마다 차트 갱신
        let now = new Date().toLocaleTimeString('ko-KR',{hour12:false});
        trendChart.data.labels.push(now);
        trendChart.data.datasets[0].data.push(t);
        trendChart.data.datasets[1].data.push(h);
        if(trendChart.data.labels.length > 20){ trendChart.data.labels.shift(); trendChart.data.datasets[0].data.shift(); trendChart.data.datasets[1].data.shift(); }
        trendChart.update('none'); chartUpdateCounter = 0;
      }
    } catch(e){}
  }
  async function relay(n){ await fetch("/relay?ch="+n); update(); } // 버튼 클릭시 실행
  initChart(); setInterval(update, 2000); update();
  </script></body></html>)rawliteral";
  server.send(200, "text/html", page);
}

// [WiFi 설정 핸들러] WiFi 정보가 없을 때 뜨는 설정 페이지
void handleSetup() {
  String page = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>" + String(FPSTR(commonCSS)) + "</style></head><body><div class='wrap'><div class='panel' style='width:min(400px,100%)'><div class='title'>WiFi Setup</div><form action='/save'><select name='s'>";
  int n = WiFi.scanNetworks(); // 주변 WiFi 검색
  if(n <= 0) page += "<option>No WiFi Found</option>";
  else { for(int i=0; i<n; i++) page += "<option value='"+WiFi.SSID(i)+"'>"+WiFi.SSID(i)+"</option>"; }
  page += "</select><input name='p' type='password' placeholder='Password'><button type='submit'>Save & Reboot</button></form></div></div></body></html>";
  server.send(200, "text/html", page);
}

// [시스템 설정 핸들러] 온도 보정이나 자동 모드 설정 페이지
void handleSettings() {
  if (!server.authenticate(www_username, www_password)) return server.requestAuthentication();
  String page = "<html><head><title>동심팜 환경제어시스템</title><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><style>" + String(FPSTR(commonCSS)) + "</style></head><body><div class='wrap'>";
  page += "<div class='card full'><div class='panel'><div class='title'>시스템 환경 설정</div><form action='/set_config' method='GET'>";
  page += "온도 보정값: <input type='number' name='d10' value='"+String(dwValues[10])+"'>";
  page += "자동 제어 모드: <select name='m200'><option value='0' "+String(!mBits[0]?"selected":"")+">수동</option><option value='1' "+String(mBits[0]?"selected":"")+">자동</option></select>";
  page += "<button type='submit' style='background:#4CAF50; margin-top:10px;'>설정 저장</button>";
  page += "<button type='button' onclick=\"location.href='/'\" style='background:#555; margin-top:10px;'>취소</button></form></div></div></div></body></html>";
  server.send(200, "text/html", page);
}

void setup() {
  Serial.begin(115200);
  // 1. 릴레이 핀 모드 설정 및 기존 상태 복구
  for(int i=0; i<16; i++) pinMode(relayPins[i], OUTPUT);
  
  prefs.begin("relay_stat", true);
  for(int i=0; i<16; i++){
    char key[5]; sprintf(key, "r%d", i);
    mBits[i] = prefs.getBool(key, false);
    digitalWrite(relayPins[i], mBits[i] ? HIGH : LOW);
  }
  prefs.end();

  // 2. 저장된 WiFi 정보 읽기
  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", ""), pass = prefs.getString("pass", "");
  prefs.end();
  
  // 3. WiFi 접속 시도
  if(ssid.length() > 0) {
    WiFi.mode(WIFI_STA); WiFi.begin(ssid.c_str(), pass.c_str());
    int retry = 0; while(WiFi.status() != WL_CONNECTED && retry < 20){ delay(500); Serial.print("."); retry++; }
  }
  
  if(WiFi.status() == WL_CONNECTED) { // 연결 성공시
    wifiConnected = true; 
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); // NTP 시간 동기화
    if (MDNS.begin("dongsim")) { // 'dongsim.local' 주소 활성화
      Serial.println("\nmDNS started: http://dongsim.local");      
      MDNS.addService("http", "tcp", 80); // 웹 서비스(http, tcp, 80포트)를 네트워크에 확실히 알림
    }
  } else { // 연결 실패시 설정 모드(AP) 시작
    WiFi.mode(WIFI_AP); WiFi.softAP("Dongsim_Farm_Setup", "35327929");
    Serial.println("\nAP Mode Started: 192.168.4.1");
  }

  // --- [웹 서버 라우팅 설정] ---
  server.on("/", handleRoot);
  server.on("/setup", handleSetup);
  server.on("/settings", handleSettings);
  
  // WiFi 정보를 저장하고 재부팅 처리
  server.on("/save", [](){
    prefs.begin("wifi", false); prefs.putString("ssid", server.arg("s")); prefs.putString("pass", server.arg("p")); prefs.end();
    server.send(200, "text/html", "Saved. Rebooting..."); delay(1000); ESP.restart();
  });

  // 개별 릴레이 제어 처리
  server.on("/relay", [](){
    int ch = server.arg("ch").toInt();
    controlRelay(ch, !mBits[ch]);
    server.send(200, "text/plain", "ok");
  });

  // 모든 데이터를 JSON 형식으로 브라우저에 전송 (핵심 데이터 통로)
  server.on("/all_data", [](){
    String json = "{\"m\":[";
    for(int i=0; i<16; i++) json += String(mBits[i]) + (i<15?",":"");
    // [센서 데이터] 실제 센서가 없다면 가상 데이터를 만들어 보냄
    float fakeTemp = 25.5 + (random(-10, 10) / 10.0);
    int fakeHumi = 50 + random(-5, 5);
    json += "],\"temp\":"+String(fakeTemp)+",\"humi\":"+String(fakeHumi)+",\"datetime\":\""+getDateTimeString()+"\",\"rssi\":\""+String(WiFi.RSSI())+" dBm\",\"heap\":\""+String(ESP.getFreeHeap()/1024)+"\"}";
    server.send(200, "application/json", json);
  });
  
  server.begin(); // 웹 서버 시작
  Serial.println("System Started!");
}

void loop() { 
  server.handleClient(); // 사용자의 웹 요청을 계속 처리
}