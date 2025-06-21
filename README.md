# Sistema de Controle de Nível de Água

<span id="topo"></span>

## **Descrição**

Sistema embarcado para monitoramento e controle automático do nível de água em reservatórios, utilizando o Raspberry Pi Pico W. O projeto integra sensores, display OLED, matriz de LEDs, botões e conectividade Wi-Fi para visualização local e remota do status do reservatório, além de acionar uma bomba d’água automaticamente conforme o nível.

---

## **Funcionalidades**

- Monitoramento do nível de água via potenciômetro de boia ou sensor ultrassônico.
- Exibição do nível de água em display OLED e matriz de LEDs.
- Controle automático da bomba d’água com base em limites configuráveis.
- Alarme sonoro (buzzer) para situações críticas.
- Interface web para visualização remota do status e configuração dos limites.
- Indicação visual do status da bomba e do sistema por LEDs.
- Botões físicos para redefinir limites e controle manual.

---

## **Requisitos**

### **Hardware**

- Raspberry Pi Pico W
- Display OLED SSD1306 (128x64)
- Matriz de LEDs WS2812B (5x5)
- Sensor de nível (potenciômetro de boia ou sensor ultrassônico)
- Relé para acionamento da bomba
- Buzzer
- Botões (mínimo 3)
- Fonte de alimentação compatível

### **Software**

- **SDK do Raspberry Pi Pico.**

- SDK do Raspberry Pi Pico
- FreeRTOS
- lwIP (TCP/IP)
- Bibliotecas:
  - ssd1306 (display OLED)
  - ws2812b (matriz de LEDs)
  - button (botões)
  - buzzer (alarme sonoro)
  - ultrasonic (opcional, para sensor ultrassônico)

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

---

## **Demonstração**

Confira o vídeo de demonstração do projeto no YouTube/Drive:
[![Demonstração]](link)

---

## 🤝 Equipe

Membros da equipe de desenvolvimento do projeto:

<table>
  <tr>
    <td align="center">
      <a href="https://github.com/matheusssilva991">
        <img src="https://github.com/matheusssilva991.png" width="100px;" alt="Foto do Matheus S.Silva no GitHub"/><br>
        <b>Matheus S.Silva</b>
        <p>Embedded Systems Developer</p>
      </a>
    </td>
     <td align="center">
      <a href="https://github.com/LeonardoBonifacio">
        <img src="https://github.com/LeonardoBonifacio.png" width="100px;" alt="Foto do Leonardo Bonifárcio no GitHub"/><br>
        <b>Leonardo Bonifácio</b>
        <p>Embedded Systems Developer</p>
      </a>
    </td>

   <td align="center">
      <a href="https://github.com/yuriccosta">
        <img src="https://github.com/yuriccosta.png" width="100px;" alt="Foto do Yuri Costa no GitHub"/><br>
        <b>Yuri Costa</b>
        <p>Embedded Systems Developer</p>
      </a>
    </td>

  </tr>
</table>

---

## **Contribuindo**

1. Faça um fork deste repositório.
2. Crie uma nova branch: `git checkout -b minha-contribuicao`.
3. Faça suas alterações e commit: `git commit -m 'Minha contribuição'`.
4. Envie para o seu fork: `git push origin minha-contribuicao`.
5. Abra um Pull Request neste repositório.

---

## **Licença**

Este projeto está licenciado sob a licença MIT. Veja o arquivo [LICENSE](LICENSE) para mais detalhes.

[⬆ Voltar ao topo](#topo)
