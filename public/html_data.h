#ifndef HTML_DATA_H
#define HTML_DATA_H

const char *html_data =
    "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Controle do LED</title>"
    "<style>"
    "body { font-family: sans-serif; text-align: center; padding: 10px; margin: 0; background:rgb(0, 0, 0); color: white}"
    ".botao { font-size: 20px; padding: 10px 30px; margin: 10px; border: none; border-radius: 8px; }"
    ".on { background: #4CAF50; color: white; }"
    ".off { background: #f44336; color: white; }"
    ".barra { width: 30%; background: #ddd; border-radius: 6px; overflow: hidden; margin: 0 auto 15px auto; height: 20px; }"

    ".preenchimento { height: 100%; transition: width 0.3s ease; }"
    "#barra_x { background: #2196F3; }"
    ".label { font-weight: bold; margin-bottom: 5px; display: block; }"
    ".bolinha { width: 20px; height: 20px; border-radius: 50%; display: inline-block; margin-left: 10px; background: #ccc; transition: background 0.3s ease; }"
    "@media (max-width: 600px) { .botao { width: 80%; font-size: 18px; } }"
    "</style>"
    "<script>"
    "function sendCommand(cmd) { fetch('/'+cmd); }"
    "function atualizar() {"
    "  fetch('/estado').then(res => res.json()).then(data => {"
    "    document.getElementById('estado').innerText = data.bomba_agua ? 'Ligado' : 'Desligado';"
    "    document.getElementById('valor_potenciometro').innerText = data.nivel_agua;"
    "    document.getElementById('barra_x').style.width = data.nivel_agua + '%';"
    "  });"
    "}"
    "setInterval(atualizar, 100);"
    "</script></head><body>"

    "<h1>Controle do Nivel de água no Reservatório</h1>"

    "<p>Estado da Bomba de água: <span id='estado'>--</span></p>"

    "<p class='label'>Nivel de água: <span id='valor_potenciometro'>--</span>%</p>"
    "<div class='barra'><div id='barra_x' class='preenchimento'></div></div>"

    "<button class='botao on' onclick=\"sendCommand('bomba/on')\">Ligar bomba</button>"
    "<button class='botao off' onclick=\"sendCommand('bomba/off')\">Desligar Bomba</button>"

    "</body></html>";

#endif // HTML_DATA_H
