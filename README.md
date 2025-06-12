# Sistema de Controle de Nível de Água

## **Descrição**

Descrição Aqui

---

## **Funcionalidades**

Funcionalidades Aqui

---

## **Requisitos**

### **Hardware**

- Microcontrolador Raspberry Pi Pico W.
- Display OLED SSD1306 (128x64 pixels).
- Matriz de LEDs WS2812B.
- Botões para controle manual.
- Fonte de alimentação compatível.

### **Software**

- **SDK do Raspberry Pi Pico.**

- **FreeRTOS** para gerenciamento de tarefas.
- **lwIP** para comunicação TCP/IP.
- **Bibliotecas adicionais:**
  - `ssd1306` para controle do display OLED.
  - `ws2812b` para controle da matriz de LEDs.
  - `button` para leitura de botões.

---

## **Como Rodar**

1. **Clone o repositório:**

   ```bash
   git clone https://github.com/seu-usuario/estacionamento-inteligente.git
   cd estacionamento-inteligente
   ```

2. **Configure o Wi-Fi:**
    - Renomeie o arquivo `wifi_config_example.h` na pasta config para `wifi_config.h`.
    - Edite o arquivo `wifi_config.h` e adicione suas credenciais Wi-Fi:

    ```c
    #define WIFI_SSID "SeuSSID"
    #define WIFI_PASSWORD "SuaSenha"
    ```

3. **Configure o caminho do FReeRTOS:**
   - Edite o arquivo `CMakeLists.txt` e defina o caminho correto para o FreeRTOS:

   ```cmake
   set(FREERTOS_PATH "/caminho/para/seu/FreeRTOS")
   ```

   - Certifique-se de que o FreeRTOS esteja corretamente instalado e acessível.
   - O caminho padrão é `~/pico/FreeRTOS`.
   - Caso tenha instalado o FreeRTOS em outro local, ajuste o caminho conforme necessário.
   - O caminho deve ser absoluto ou relativo ao diretório do projeto.
   - Exemplo de caminho absoluto: `/home/usuario/pico/FreeRTOS`.

4. **Compile e envie o código para o Raspberry Pi Pico W:**

   ```bash
   mkdir build
   cd build
   cmake -G "Ninja" ..
   ninja
   ```

## **Demonstração**

Confira o vídeo de demonstração do projeto no YouTube/Drive:
[![Demonstração]](link)

## **Contribuindo**

1. Faça um fork deste repositório.
2. Crie uma nova branch: `git checkout -b minha-contribuicao`.
3. Faça suas alterações e commit: `git commit -m 'Minha contribuição'`.
4. Envie para o seu fork: `git push origin minha-contribuicao`.
5. Abra um Pull Request neste repositório.

---

## **Licença**

Este projeto está licenciado sob a licença MIT. Veja o arquivo [LICENSE](LICENSE) para mais detalhes.