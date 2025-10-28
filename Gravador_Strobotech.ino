// Arquivo principal: strobotech_ota_flash.ino (substitua/cole no seu projeto)
#include "flash.h"
#include "spiffsConfig.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <SPIFFS.h>
#include <Update.h>

const char* VERSION_INFO = "Grupo Alfas - 2025 - V1.0";

// =================== CONFIGURAÇÕES ===================
const char* ssidOTA = "StroboTech";
const char* senhaOTA = "12345678";
const byte DNS_PORT = 53;

DNSServer dnsServer;
WebServer server(80);
bool otaAtivo = false;

// =================== HTML da interface ===================
const char* htmlAdm = R"rawliteral(
  <!DOCTYPE html>
  <html lang="pt">
  <head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
  <title>Gerenciamento Strobotech</title>
  <style>
  body {font-family: Arial, sans-serif; background:#f0f0f0; text-align:center; margin:0; padding:0;padding-bottom: 50px;}
  .header {background:#333; color:#fff; padding:10px 0;}
  .container {max-width:800px;background:#fff;padding:20px;margin:20px auto;border-radius:10px;box-shadow:0 4px 10px rgba(0,0,0,0.1);}
  .tab {display:flex; justify-content:center; background:#222;}
  .tab button {background:inherit;color:#fff;border:none;padding:12px 20px;cursor:pointer;font-size:16px;}
  .tab button:hover {background:#444;}
  .tab button.active {background:#4CAF50;}
  .tabcontent {display:none; padding:20px;}
  input[type='file'], input[type='number'], textarea {width:100%; box-sizing:border-box; margin:8px 0; padding:8px; font-size:14px;}
  textarea {height:160px; resize:vertical;}
  button, input[type='submit'] {background-color:#4CAF50;color:#fff;border:none;padding:10px 20px;border-radius:5px;cursor:pointer;font-size:16px;}
  button:hover {background-color:#45a049;}
  .progress{width:100%;background:#ddd;border-radius:5px;margin-top:10px;}
  .progress-bar{width:0%;height:20px;background:#4CAF50;color:#fff;border-radius:5px;text-align:center;transition:width .3s;}
  .small {font-size:13px;color:#666;margin-top:8px;}
  .row {display:flex; gap:10px;}
  .col {flex:1;}
  .status {margin-top:10px;padding:8px;border-radius:6px;background:#fafafa;border:1px solid #eee;}
  footer {
    position: fixed;
    bottom: 0;
    left: 0;
    width: 100%;
    background-color: #222;
    color: white;
    text-align: center;
    padding: 10px 0;
    font-size: 14px;
  }
  </style>
  </head>
  <body>
  <div class="header"><h1 style="margin:0;padding:6px 0;">STROBOTECH - Gerenciamento</h1></div>

  <div class="tab">
    <button class="tablinks active" onclick="openTab(event,'ota')">OTA / Arquivos</button>
    <button class="tablinks" onclick="openTab(event,'flash')">Gravar Flash</button>
  </div>

  <div id="ota" class="tabcontent" style="display:block;">
    <div class="container">
      <h2>Atualização OTA do Firmware</h2>
      <form id="uploadForm">
        <input type="file" id="updateFile" accept=".bin" required><br>
        <input type="submit" value="Atualizar Firmware">
        <div class="progress"><div id="progressBar" class="progress-bar">0%</div></div>
      </form>
      <div style="margin-top:20px;">
        <h3>Arquivos BIN</h3>
        <div id="fileList">Carregando...</div>
      </div>
    </div>
  </div>

  <div id="flash" class="tabcontent">
    <div class="container">
      <h2>Gravar texto na Flash Externa</h2>
      <p class="small">Informe o endereço de início (decimal ou hex com 0x) e o texto. Pressione <strong>Gravar</strong>.</p>
      <p class="small">Slots:  <strong>0x0000</strong> = Logo alfas, <strong>0x2000</strong> = Logo Senai, <strong>0x3000</strong> = Trofeu, <strong>0x5000</strong> = Dados e livre de <strong>0x7000</strong> até <strong>0x9FFF</strong>.</p>
      <div class="row">
        <div class="col">
          <label for="flashAddress">Endereço inicial:</label>
          <input type="text" id="flashAddress" placeholder="Ex: 131072 ou 0x0000" value="0x0000">
        </div>
      </div>
      <label for="flashText">Texto a gravar:</label>
      <textarea id="flashText" placeholder="Cole aqui o texto que deseja gravar na flash..."></textarea>
      <button id="writeFlashBtn">Gravar na Flash</button>
      <div id="flashStatus" class="status">Pronto.</div>
    </div>
  </div>

  <script>
  /* Controle de abas */
  function openTab(evt, tabName) {
    const tabcontent = document.getElementsByClassName('tabcontent');
    const tablinks = document.getElementsByClassName('tablinks');
    for (let i = 0; i < tabcontent.length; i++) tabcontent[i].style.display = 'none';
    for (let i = 0; i < tablinks.length; i++) tablinks[i].classList.remove('active');
    document.getElementById(tabName).style.display = 'block';
    evt.currentTarget.classList.add('active');

    if (tabName === 'ota') loadFiles();
  }

  /* Upload OTA */
  const form=document.getElementById('uploadForm');
  const progressBar=document.getElementById('progressBar');
  form.addEventListener('submit',function(e){
    e.preventDefault();
    const file=document.getElementById('updateFile').files[0];
    if(!file)return alert('Selecione um arquivo .bin');
    const xhr=new XMLHttpRequest();
    const formData=new FormData();
    formData.append('update',file);
    xhr.open('POST','/update',true);
    xhr.upload.onprogress=function(event){
      if(event.lengthComputable){
        const percent=Math.round((event.loaded/event.total)*100);
        progressBar.style.width=percent+'%';
        progressBar.textContent=percent+'%';
      }
    };
    xhr.onload=function(){
      if(xhr.status===200){
        progressBar.style.width='100%';
        progressBar.textContent='Upload concluído!';
        alert('Atualização enviada! Reiniciando...');
      } else {
        alert('Erro na atualização!');
      }
    };
    xhr.send(formData);
  });
  /* Gravar na flash via POST urlencoded */
  document.getElementById('writeFlashBtn').addEventListener('click', function(){
    const addr = document.getElementById('flashAddress').value.trim();
    const text = document.getElementById('flashText').value;
    const status = document.getElementById('flashStatus');

    if (!addr) { status.textContent = 'Informe o endereço inicial.'; return; }
    if (!text) { status.textContent = 'Informe o texto a gravar.'; return; }

    status.textContent = 'Enviando dados para gravação...';

    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/writeflash', true);
    xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded; charset=UTF-8');
    xhr.onreadystatechange = function() {
      if (xhr.readyState === XMLHttpRequest.DONE) {
        if (xhr.status === 200) {
          status.textContent = '✅ Gravado com sucesso.';
        } else {
          status.textContent = '❌ Erro: ' + xhr.responseText;
        }
      }
    };
    const body = 'address=' + encodeURIComponent(addr) + '&text=' + encodeURIComponent(text);
    xhr.send(body);
  });
  </script>
  <footer><div id="footer" style="margin-top:30px; padding:10px; background:#333; color:white; font-size:14px;">
    Carregando versão...
  </div></footer>

  <script>
  window.addEventListener('load', () => {
    fetch('/version')
      .then(r => r.text())
      .then(v => { document.getElementById('footer').textContent = v; })
      .catch(() => { document.getElementById('footer').textContent = 'Versão indisponível'; });
  });
  </script>
  </body>
  </html>
  )rawliteral";
// =================== Funções auxiliares ===================
const char *htmlClient = R"rawliteral(
      <!DOCTYPE html>
      <html lang="pt">
      <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Gerenciamento Strobotech</title>
      <style>
      body {
        font-family: Arial, sans-serif;
        background-color: #f0f0f0;
        text-align: center;
        margin: 0;
        padding: 0;
        padding-bottom: 50px;
      }
      .container {
        max-width: 500px;
        background: #fff;
        padding: 20px;
        margin: 30px auto;
        border-radius: 10px;
        box-shadow: 0px 4px 10px rgba(0,0,0,0.1);
      }
      h2 {
        color: #333;
      }
      input[type='file'] {
        margin: 15px 0;
        width: 100%;
      }
      input[type='submit'], button {
        background-color: #4CAF50;
        color: white;
        border: none;
        padding: 10px 20px;
        font-size: 16px;
        border-radius: 5px;
        cursor: pointer;
        margin-top: 10px;
      }
      input[type='submit']:hover, button:hover {
        background-color: #45a049;
      }
      .progress {
        width: 100%;
        background-color: #ddd;
        border-radius: 5px;
        margin-top: 10px;
      }
      .progress-bar {
        width: 0%;
        height: 20px;
        background-color: #4CAF50;
        border-radius: 5px;
        text-align: center;
        color: white;
        transition: width 0.3s;
      }
      #fileList {
        margin-top: 20px;
      }
      .file-item {
        background: #fafafa;
        margin: 5px 0;
        padding: 10px;
        border-radius: 5px;
        box-shadow: 0px 1px 3px rgba(0,0,0,0.1);
        display: flex;
        justify-content: space-between;
        align-items: center;
      }
      .file-item button {
        background-color: #2196F3;
        font-size: 14px;
        padding: 5px 10px;
      }
      .file-item button.delete {
        background-color: #f44336;
      }
      .file-item button:hover {
        opacity: 0.9;
      }
      p {
        color: #555;
        font-size: 14px;
        margin: 10px 0;
      }

      /* ===== Estilo das Abas ===== */
      .tab {
        overflow: hidden;
        background-color: #333;
        display: flex;
        justify-content: center;
      }
      .tab button {
        background-color: inherit;
        border: none;
        outline: none;
        cursor: pointer;
        padding: 14px 20px;
        color: white;
        transition: background-color 0.3s;
        font-size: 16px;
      }
      .tab button:hover {
        background-color: #575757;
      }
      .tab button.active {
        background-color: #4CAF50;
      }
      .tabcontent {
        display: none;
      }
      footer {
        position: fixed;
        bottom: 0;
        left: 0;
        width: 100%;
        background-color: #222;
        color: white;
        text-align: center;
        padding: 10px 0;
        font-size: 14px;
      }
      </style>
      </head>
      <body>

      <!-- Menu de Abas -->
      <div class="tab">
        <button class="tablinks active" onclick="openTab(event, 'arquivos')">Arquivos</button>
        <button class="tablinks" onclick="openTab(event, 'update')">Atualização OTA</button>
      </div>

      <!-- ABA 1 - LISTAR ARQUIVOS -->
      <div id="arquivos" class="tabcontent" style="display:block;">
        <div class="container">
          <h2>Arquivos CSV do Vibrometro</h2>
          <div id="fileList">Carregando...</div>
        </div>
      </div>

      <!-- ABA 2 - ATUALIZAÇÃO OTA -->
      <div id="update" class="tabcontent">
        <div class="container">
          <h2>Atualização OTA do Firmware</h2>
          <p>Escolha o arquivo .bin do firmware para atualizar seu dispositivo</p>
          <form id="uploadForm">
            <input type="file" name="update" id="updateFile" accept=".bin" required><br>
            <input type="submit" value="Atualizar Firmware">
            <div class="progress"><div class="progress-bar" id="progressBar">0%</div></div>
          </form>
          <p>Atenção: Não desligue o dispositivo durante a atualização!</p>
        </div>
      </div>

      <script>
      const form = document.getElementById('uploadForm');
      const progressBar = document.getElementById('progressBar');
      const fileList = document.getElementById('fileList');

      // ---- Controle das Abas ----
      function openTab(evt, tabName) {
        const tabcontent = document.getElementsByClassName('tabcontent');
        const tablinks = document.getElementsByClassName('tablinks');

        for (let i = 0; i < tabcontent.length; i++) {
          tabcontent[i].style.display = 'none';
        }
        for (let i = 0; i < tablinks.length; i++) {
          tablinks[i].classList.remove('active');
        }
        document.getElementById(tabName).style.display = 'block';
        evt.currentTarget.classList.add('active');

        // Recarregar lista quando abrir aba de arquivos
        if (tabName === 'arquivos') loadFiles();
      }

      // ---- Upload OTA ----
      form.addEventListener('submit', function(e) {
        e.preventDefault();
        const file = document.getElementById('updateFile').files[0];
        if (!file) return alert('Selecione um arquivo .bin');
        const xhr = new XMLHttpRequest();
        const formData = new FormData();
        formData.append('update', file);
        xhr.open('POST', '/update', true);
        xhr.upload.onprogress = function(event) {
          if (event.lengthComputable) {
            const percent = Math.round((event.loaded / event.total) * 100);
            progressBar.style.width = percent + '%';
            progressBar.textContent = percent + '%';
          }
        };
        xhr.onload = function() {
          if (xhr.status === 200) {
            progressBar.style.width = '100%';
            progressBar.textContent = 'Upload concluído!';
            alert('Atualização enviada! O dispositivo reiniciará se a OTA for bem-sucedida.');
          } else {
            alert('Erro na atualização!');
          }
        };
        xhr.send(formData);
      });

      // ---- Listar arquivos CSV ----
      function loadFiles() {
        fetch('/list')
          .then(response => response.json())
          .then(files => {
            if (files.length === 0) {
              fileList.innerHTML = "<p>Nenhum arquivo CSV encontrado.</p>";
              return;
            }
            fileList.innerHTML = "";
            files.forEach(file => {
              const div = document.createElement('div');
              div.classList.add('file-item');
              div.innerHTML = `
                <span>${file.name} (${file.size} bytes)</span>
                <div>
                  <button onclick="downloadFile('${file.name}')">Baixar</button>
                  <button class="delete" onclick="deleteFile('${file.name}')">Excluir</button>
                </div>
              `;
              fileList.appendChild(div);
            });
          })
          .catch(() => {
            fileList.innerHTML = "<p>Erro ao carregar lista de arquivos.</p>";
          });
      }

      // ---- Download de arquivo ----
      function downloadFile(filename) { 
        var urlDoArquivo = '/download?file=' + encodeURIComponent(filename);
        
        var linkTemporario = document.createElement('a');
        linkTemporario.href = urlDoArquivo;
        linkTemporario.download = filename;
        
        document.body.appendChild(linkTemporario);
        linkTemporario.click();
        document.body.removeChild(linkTemporario);
      };

      // ---- Excluir arquivo ----
      function deleteFile(filename) {
        if (!confirm('Deseja realmente excluir ' + filename + '?')) return;
        fetch('/delete?file=' + encodeURIComponent(filename))
          .then(response => response.text())
          .then(msg => {
            alert(msg);
            loadFiles();
          })
          .catch(() => alert('Erro ao excluir arquivo.'));
      }

      // ---- Carrega lista ao abrir ----
      window.onload = loadFiles;
      </script>
      <footer><div id="footer" style="margin-top:30px; padding:10px; background:#333; color:white; font-size:14px;">
        Carregando versão...
      </div></footer>

      <script>
      window.addEventListener('load', () => {
        fetch('/version')
          .then(r => r.text())
          .then(v => { document.getElementById('footer').textContent = v; })
          .catch(() => { document.getElementById('footer').textContent = 'Versão indisponível'; });
      });
      </script>
      </body>
      </html>
      )rawliteral";
// Grava indexadm.html no SPIFFS se não existir
void salvarHTMLnoSPIFFS(const char* ScriptHtml, String nomeHtml) {
  if (!SPIFFS.begin(true)) {
    Serial.println("❌ Falha ao montar SPIFFS!");
    return;
  }

  bool precisaAtualizar = false;

  if (SPIFFS.exists("/"+nomeHtml)) {
    File f = SPIFFS.open("/"+nomeHtml, "r");
    if (!f) {
      Serial.println("⚠️ Erro ao abrir /indexadm.html para leitura — será regravado.");
      precisaAtualizar = true;
    } else {
      // Lê o conteúdo existente
      String conteudoAtual;
      while (f.available()) conteudoAtual += (char)f.read();
      f.close();

      // Compara tamanho e conteúdo
      if (conteudoAtual.length() != strlen(ScriptHtml)) {
        precisaAtualizar = true;
        Serial.println("🔄 indexadm.html diferente (tamanho mudou) — atualizando...");
      } else if (conteudoAtual != ScriptHtml) {
        precisaAtualizar = true;
        Serial.println("🔄 indexadm.html diferente (conteúdo mudou) — atualizando...");
      } else {
        Serial.println("✅ indexadm.html está atualizado — nenhuma ação necessária.");
      }
    }
  } else {
    Serial.println("📄 indexadm.html não existe — criando...");
    precisaAtualizar = true;
  }

  if (precisaAtualizar) {
    File f = SPIFFS.open("/"+nomeHtml, "w");
    if (!f) {
      Serial.println("❌ Erro ao criar /indexadm.html!");
      return;
    }
    f.print(ScriptHtml);
    f.close();
    Serial.println("✅ indexadm.html gravado/atualizado com sucesso!");
  }
}
// =================== Rota de escrita na flash ===================
void configurarRotaWriteFlash() {
  // Handler para gravar texto na flash (POST urlencoded: address=...&text=...)
  server.on("/writeflash", HTTP_POST, []() {
    // Se os parâmetros vierem como application/x-www-form-urlencoded,
    // o WebServer permite acessar com server.arg("address") e server.arg("text")
    if (!server.hasArg("address") || !server.hasArg("text")) {
      server.send(400, "text/plain", "Faltando parametros: address ou text");
      return;
    }

    String addrStr = server.arg("address");
    String text = server.arg("text");

    // interpretar endereço (dec ou hex 0x)
    uint32_t addr = 0;
    if (addrStr.startsWith("0x") || addrStr.startsWith("0X")) {
      addr = (uint32_t) strtoul(addrStr.c_str(), NULL, 16);
    } else {
      addr = (uint32_t) strtoul(addrStr.c_str(), NULL, 10);
    }

    size_t len = text.length();
    if (len == 0) {
      server.send(400, "text/plain", "Texto vazio");
      return;
    }

    // Limites básicos: evita overflow (ajuste conforme sua flash)
    // Se tiver macro FLASH_TOTAL_SIZE em flash.h, você pode validar:
    #ifdef FLASH_TOTAL_SIZE
      if (addr + len > FLASH_TOTAL_SIZE) {
        server.send(400, "text/plain", "Tamanho excede capacidade da flash");
        return;
      }
    #endif

    Serial.printf("Solicitado gravação na flash: addr=0x%06X len=%u\n", addr, (unsigned)len);

    // Calcular setores a apagar
    uint32_t sectorStart = (addr / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;
    uint32_t sectorEnd = ((addr + len + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;

    Serial.printf("Apagando setores de 0x%06X até 0x%06X\n", sectorStart, sectorEnd - 1);
    for (uint32_t a = sectorStart; a < sectorEnd; a += FLASH_SECTOR_SIZE) {
      eraseSector(a);
    }

    // Gravar os dados (texto como bytes)
    const uint8_t* data = (const uint8_t*)text.c_str();
    writePage(addr, data, len);

    // Verificação simples: ler de volta e comparar (apenas se len razoável)
    bool ok = true;
    if (len <= 65536) { // limite de verificação para não consumir muita RAM
      uint8_t *buf = (uint8_t*)malloc(len);
      if (buf) {
        readData(addr, buf, len);
        for (size_t i = 0; i < len; ++i) {
          if (buf[i] != (uint8_t)data[i]) {
            ok = false;
            break;
          }
        }
        free(buf);
      } else {
        Serial.println("⚠️ Não foi possível alocar buffer para verificação, pulando verificação.");
      }
    } else {
      Serial.println("⚠️ Tamanho grande — verificação pulada.");
    }

    if (ok) {
      server.send(200, "text/plain", "Gravacao concluida");
      Serial.println("✅ Gravacao concluida e verificada");
    } else {
      server.send(500, "text/plain", "Erro na verificacao apos escrita");
      Serial.println("❌ Erro na verificacao apos escrita");
    }
  });
}
// =================== Funções OTA / Captive Portal ===================
void configurarRoutesBasicas() {
  // Serve index e estáticas
  server.serveStatic("/", SPIFFS, "/indexadm.html");

  // OTA upload
  server.on(
    "/update", HTTP_POST,
    []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", Update.hasError() ? "Falha!" : "Sucesso!");
      delay(1000);
      ESP.restart();
    },
    []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Atualizando: %s\n", upload.filename.c_str());
        if (!Update.begin()) Update.printError(Serial);
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
          Update.printError(Serial);
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true))
          Serial.printf("Atualização concluída (%u bytes)\n", upload.totalSize);
      }
    });
    // NotFound -> redireciona para página principal (ajuda captive portal)
  server.onNotFound([]() {
    server.sendHeader("Location", "http://192.168.4.1", true);
    server.send(302, "text/plain", "");
  });
  server.on("/version", HTTP_GET, []() {
    server.send(200, "text/plain", VERSION_INFO);
  });
}
// Inicia AP + DNS (captive portal) e servidor
void iniciarOTA_AP() {
  if (otaAtivo) return;
  otaAtivo = true;

  Serial.println("\n=== Iniciando OTA Access Point ===");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssidOTA, senhaOTA);
  IPAddress ip = WiFi.softAPIP();
  Serial.print("AP ativo em: ");
  Serial.println(ip);

  // Captive portal: responde DNS para qualquer domínio apontando para o AP
  dnsServer.start(DNS_PORT, "*", ip);

  // Configura rotas (servir index, OTA, /writeflash, etc.)
  configurarRoutesBasicas();
  configurarRotaWriteFlash();

  // Inicia servidor
  server.begin();
  Serial.println("Servidor OTA iniciado!");
}
void processarOTA() {
  if (!otaAtivo) return;
  dnsServer.processNextRequest();
  server.handleClient();
}
void pararOTA_AP() {
  if (!otaAtivo) return;
  otaAtivo = false;
  dnsServer.stop();
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("OTA desativado (parado manualmente)");
}
// =================== Setup / Loop ===================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Inicializando...");

  // Grava automaticamente o indexadm.html no SPIFFS se não existir
  salvarHTMLnoSPIFFS(htmlAdm, "indexadm.html");
  salvarHTMLnoSPIFFS(htmlClient, "index.html");

  // Inicializa SPI e flash externa (funções do seu flash.h)
  iniciarSPIFlash();
  identificarJEDEC();

  listDir(SPIFFS, "/", 0);
  
}
void loop() {
  iniciarOTA_AP();
  processarOTA();
}
