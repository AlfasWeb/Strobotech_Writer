#include "flash.h"

void iniciarSPIFlash() {
  pinMode(PIN_CS, OUTPUT);
  digitalWrite(PIN_CS, HIGH);
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI);
  delay(100);
}
void identificarJEDEC() {
  digitalWrite(PIN_CS, LOW);
  SPI.transfer(0x9F);
  uint8_t mf = SPI.transfer(0x00);
  uint8_t dev = SPI.transfer(0x00);
  uint8_t cap = SPI.transfer(0x00);
  digitalWrite(PIN_CS, HIGH);

  Serial.print("JEDEC ID: ");
  Serial.print(mf, HEX); Serial.print(" ");
  Serial.print(dev, HEX); Serial.print(" ");
  Serial.println(cap, HEX);
}
void enableWrite() {
  digitalWrite(PIN_CS, LOW);
  SPI.transfer(0x06);
  digitalWrite(PIN_CS, HIGH);
}
void waitBusy() {
  digitalWrite(PIN_CS, LOW);
  SPI.transfer(0x05);
  while (SPI.transfer(0x00) & 0x01);
  digitalWrite(PIN_CS, HIGH);
}
void eraseSector(uint32_t addr) {
  enableWrite();
  digitalWrite(PIN_CS, LOW);
  SPI.transfer(0x20);
  SPI.transfer((addr >> 16) & 0xFF);
  SPI.transfer((addr >> 8) & 0xFF);
  SPI.transfer(addr & 0xFF);
  digitalWrite(PIN_CS, HIGH);
  waitBusy();
}
/*void writePage(uint32_t addr, const uint8_t* data, size_t len) {
  while (len > 0) {
    uint32_t pageStart = addr & ~(FLASH_PAGE_SIZE - 1);
    uint32_t pageOffset = addr - pageStart;
    uint32_t spaceInPage = FLASH_PAGE_SIZE - pageOffset;
    size_t toWrite = std::min(len, (size_t)spaceInPage);

    enableWrite();
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(0x02);
    SPI.transfer((addr >> 16) & 0xFF);
    SPI.transfer((addr >> 8) & 0xFF);
    SPI.transfer(addr & 0xFF);
    for (size_t i = 0; i < toWrite; i++) {
      SPI.transfer(data[i]);
    }
    digitalWrite(PIN_CS, HIGH);
    waitBusy();

    addr += toWrite;
    data += toWrite;
    len -= toWrite;
  }
}*/
void writePage(uint32_t addr, const uint8_t* data, size_t len) {
  // Copiamos os dados recebidos para uma string temporária
  String texto = "";
  for (size_t i = 0; i < len; i++) {
    texto += (char)data[i];
  }

  // Remover espaços extras
  texto.trim();

  // Detectar se o conteúdo é hexadecimal (começa com "0x" ou contém vírgulas separando bytes)
  bool isHex = false;
  if (texto.startsWith("0x") || texto.startsWith("0X") || texto.indexOf(",0x") != -1 || texto.indexOf(",0X") != -1) {
    isHex = true;
  }

  if (isHex) {
    Serial.println("📘 Detectado formato hexadecimal — convertendo e gravando como binário...");

    // Converter string de "0x00,0xFF..." para bytes
    std::vector<uint8_t> bin;
    int start = 0;

    while (start < texto.length()) {
      int pos = texto.indexOf(',', start);
      String token;
      if (pos == -1) {
        token = texto.substring(start);
        start = texto.length();
      } else {
        token = texto.substring(start, pos);
        start = pos + 1;
      }

      token.trim();
      if (token.startsWith("0x") || token.startsWith("0X")) token = token.substring(2);

      if (token.length() > 0) {
        int value = (int) strtol(token.c_str(), NULL, 16);
        bin.push_back((uint8_t)value);
      }
    }

    Serial.printf("📦 Total de %d bytes convertidos.\n", bin.size());

    // Grava em páginas (respeitando o limite da flash)
    const uint8_t* binData = bin.data();
    size_t binLen = bin.size();

    while (binLen > 0) {
      uint32_t pageStart = addr & ~(FLASH_PAGE_SIZE - 1);
      uint32_t pageOffset = addr - pageStart;
      uint32_t spaceInPage = FLASH_PAGE_SIZE - pageOffset;
      size_t toWrite = std::min(binLen, (size_t)spaceInPage);

      enableWrite();
      digitalWrite(PIN_CS, LOW);
      SPI.transfer(0x02); // comando de gravação
      SPI.transfer((addr >> 16) & 0xFF);
      SPI.transfer((addr >> 8) & 0xFF);
      SPI.transfer(addr & 0xFF);
      for (size_t i = 0; i < toWrite; i++) {
        SPI.transfer(binData[i]);
      }
      digitalWrite(PIN_CS, HIGH);
      waitBusy();

      addr += toWrite;
      binData += toWrite;
      binLen -= toWrite;
    }

    Serial.println("✅ Escrita de dados binários concluída.");

  } else {
    Serial.println("✏️ Gravando texto puro na flash...");

    // Grava o texto ASCII diretamente
    while (len > 0) {
      uint32_t pageStart = addr & ~(FLASH_PAGE_SIZE - 1);
      uint32_t pageOffset = addr - pageStart;
      uint32_t spaceInPage = FLASH_PAGE_SIZE - pageOffset;
      size_t toWrite = std::min(len, (size_t)spaceInPage);

      enableWrite();
      digitalWrite(PIN_CS, LOW);
      SPI.transfer(0x02);
      SPI.transfer((addr >> 16) & 0xFF);
      SPI.transfer((addr >> 8) & 0xFF);
      SPI.transfer(addr & 0xFF);
      for (size_t i = 0; i < toWrite; i++) {
        SPI.transfer(data[i]);
      }
      digitalWrite(PIN_CS, HIGH);
      waitBusy();

      addr += toWrite;
      data += toWrite;
      len -= toWrite;
    }

    Serial.println("✅ Escrita de texto concluída.");
  }
}
void readData(uint32_t addr, uint8_t* buffer, size_t len) {
  digitalWrite(PIN_CS, LOW);
  SPI.transfer(0x03);
  SPI.transfer((addr >> 16) & 0xFF);
  SPI.transfer((addr >> 8) & 0xFF);
  SPI.transfer(addr & 0xFF);
  for (size_t i = 0; i < len; i++) {
    buffer[i] = SPI.transfer(0x00);
  }
  digitalWrite(PIN_CS, HIGH);
}
void limparFlashParcial(uint32_t addrIni, uint32_t addrFim) {
  if (addrFim <= addrIni) return;
  uint8_t paginaVazia[FLASH_PAGE_SIZE];
  memset(paginaVazia, 0xFF, FLASH_PAGE_SIZE);

  while (addrIni < addrFim) {
    size_t pagina = std::min((size_t)FLASH_PAGE_SIZE, (size_t)(addrFim - addrIni));
    writePage(addrIni, paginaVazia, pagina);
    addrIni += pagina;
  }
}
bool writeFlashFromStrings(String &addrStr, String &text) {
  addrStr.trim();
  text.trim();

  // --- Converte o endereço ---
  uint32_t addr = 0;
  if (addrStr.startsWith("0x") || addrStr.startsWith("0X")) {
    addr = (uint32_t)strtoul(addrStr.c_str(), NULL, 16);
  } else {
    addr = (uint32_t)addrStr.toInt();
  }

  if (addr == 0 && !addrStr.equalsIgnoreCase("0")) {
    Serial.println("⚠️ Endereço inválido.");
    return false;
  }

  // --- Detecta se o texto é hexadecimal ---
  bool isHex = (text.startsWith("0x") || text.startsWith("0X") || 
                text.indexOf(",0x") != -1 || text.indexOf(",0X") != -1);

  if (isHex) {
    Serial.println("📘 Detectado formato hexadecimal — convertendo e gravando como binário...");

    // Converter string "0x00,0xFF..." para bytes
    std::vector<uint8_t> bin;
    int start = 0;

    while (start < text.length()) {
      int pos = text.indexOf(',', start);
      String token;
      if (pos == -1) {
        token = text.substring(start);
        start = text.length();
      } else {
        token = text.substring(start, pos);
        start = pos + 1;
      }

      token.trim();
      if (token.startsWith("0x") || token.startsWith("0X"))
        token = token.substring(2);

      if (token.length() > 0) {
        char *endptr;
        long value = strtol(token.c_str(), &endptr, 16);
        if (*endptr == '\0' && value >= 0 && value <= 255) {
          bin.push_back((uint8_t)value);
        } else {
          Serial.printf("⚠️ Byte inválido ignorado: '%s'\n", token.c_str());
        }
      }
    }

    Serial.printf("📦 Total de %d bytes convertidos.\n", bin.size());
    if (bin.empty()) {
      Serial.println("❌ Nenhum byte válido detectado.");
      return false;
    }

    // --- Grava binário ---
    const uint8_t* binData = bin.data();
    size_t binLen = bin.size();

    while (binLen > 0) {
      uint32_t pageStart = addr & ~(FLASH_PAGE_SIZE - 1);
      uint32_t pageOffset = addr - pageStart;
      uint32_t spaceInPage = FLASH_PAGE_SIZE - pageOffset;
      size_t toWrite = std::min(binLen, (size_t)spaceInPage);

      enableWrite();
      digitalWrite(PIN_CS, LOW);
      SPI.transfer(0x02); // Comando de gravação
      SPI.transfer((addr >> 16) & 0xFF);
      SPI.transfer((addr >> 8) & 0xFF);
      SPI.transfer(addr & 0xFF);

      for (size_t i = 0; i < toWrite; i++) {
        SPI.transfer(binData[i]);
      }

      digitalWrite(PIN_CS, HIGH);
      waitBusy();

      addr += toWrite;
      binData += toWrite;
      binLen -= toWrite;
    }

    Serial.println("✅ Escrita binária concluída com sucesso.");
  }

  else {
    Serial.println("✏️ Gravando texto puro na flash...");

    const uint8_t* data = (const uint8_t*)text.c_str();
    size_t len = text.length();

    while (len > 0) {
      uint32_t pageStart = addr & ~(FLASH_PAGE_SIZE - 1);
      uint32_t pageOffset = addr - pageStart;
      uint32_t spaceInPage = FLASH_PAGE_SIZE - pageOffset;
      size_t toWrite = std::min(len, (size_t)spaceInPage);

      enableWrite();
      digitalWrite(PIN_CS, LOW);
      SPI.transfer(0x02);
      SPI.transfer((addr >> 16) & 0xFF);
      SPI.transfer((addr >> 8) & 0xFF);
      SPI.transfer(addr & 0xFF);

      for (size_t i = 0; i < toWrite; i++) {
        SPI.transfer(data[i]);
      }

      digitalWrite(PIN_CS, HIGH);
      waitBusy();

      addr += toWrite;
      data += toWrite;
      len -= toWrite;
    }

    Serial.println("✅ Escrita de texto concluída com sucesso.");
  }

  return true;
}
