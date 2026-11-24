# Theremidi HC-SR04 — Arduino Leonardo (ATmega32U4)

Firmware para dois sensores ultrassônicos HC-SR04 e display OLED SSD1306, utilizando o USB nativo do Arduino Leonardo como dispositivo MIDI.

## Modos independentes dos sensores

O botão **D4** alterna o sensor selecionado (Sensor 1 / Sensor 2). O botão **D5** percorre os quatro modos do sensor selecionado:

1. **NOTES** — gera notas MIDI com snapping para a escala selecionada.
2. **VELOCITY** — altera a velocity global (1–127), utilizada nos próximos eventos Note On.
3. **CC** — envia um MIDI Control Change com valor de 0–127.
4. **PITCH** — envia uma nota-base e altera continuamente sua afinação por mensagens MIDI Pitch Bend.

A sequência é `NOTES → VELOCITY → CC → PITCH → NOTES`. Cada sensor mantém seu próprio modo. A configuração inicial permanece Sensor 1 = NOTES e Sensor 2 = CC.

## Parâmetros globais e individuais

| Parâmetro | Escopo | Controles |
|---|---|---|
| Modo de operação | Individual por sensor | D5, após selecionar com D4 |
| Número CC | Individual por sensor | D6 / D7 |
| Oitava | Individual por sensor | D8 / D9 |
| Canal MIDI (1–16) | Individual por sensor | D10 / D11 |
| Nota-base (C–B) | Global | D12 / D13 |
| Escala | Global | A4 |
| Velocity | Global | Sensor em modo VELOCITY |
| Sustain | Global, aplicado aos canais configurados | A5 |

O Sensor 1 e o Sensor 2 começam no canal MIDI 1 e com deslocamento de oitava zero, preservando o comportamento inicial anterior. Apesar dos valores iniciais iguais, canal e oitava são armazenados independentemente. Por exemplo, o Sensor 1 pode enviar no canal 1 e na oitava central, enquanto o Sensor 2 envia no canal 2 e uma oitava abaixo.

A nota-base e a escala continuam globais. Assim, ao utilizar os dois sensores em NOTES, cada um pode tocar em oitavas e canais diferentes, mas ambos seguem a mesma tonalidade e escala.

### NOTES e polifonia

Cada sensor pode manter uma nota ativa. Portanto, os dois sensores podem enviar duas notas simultâneas, inclusive em canais MIDI diferentes. O firmware também evita enviar Note On/Off redundante quando os dois sensores mantêm a mesma nota no mesmo canal.

A velocity permanece global: um sensor em VELOCITY altera a velocity utilizada pelos próximos Note On dos dois sensores. A alteração não muda a velocity de notas que já estão tocando.

### PITCH — modo contínuo

Ao entrar no modo PITCH, o sensor inicia uma nota-base definida pela nota-base global e pela oitava individual do sensor. A distância da mão é convertida para Pitch Bend:

- Mão próxima: bend positivo máximo.
- Centro da faixa: afinação central.
- Mão distante: bend negativo máximo.

O firmware envia RPN 0,0 para configurar a sensibilidade de Pitch Bend do canal em **±12 semitons**. Assim, a varredura completa do sensor corresponde a duas oitavas em torno da nota-base. Alguns instrumentos virtuais podem ignorar essa configuração RPN; nesses casos, configure manualmente a faixa de Pitch Bend do instrumento para ±12 semitons.

Pitch Bend é uma mensagem por canal MIDI. Se os dois sensores utilizarem PITCH no mesmo canal, ambos compartilharão o mesmo controle de afinação. Para controlar dois instrumentos de forma independente, configure canais diferentes — por exemplo, canal 1 para o Sensor 1 e canal 2 para o Sensor 2.

O sensor é atualizado aproximadamente a cada 120 ms, pois os disparos dos dois HC-SR04 são alternados para reduzir interferência acústica. O modo PITCH funciona com essa frequência, mas a resposta pode parecer escalonada em movimentos rápidos. Aumentar muito a frequência de disparo pode provocar leituras cruzadas entre os sensores; a montagem física deve ser testada antes de reduzir esse intervalo.

## Escalas

Todas as escalas cobrem a mesma extensão de **duas oitavas** (0 a 24 semitons). A quantidade de posições depende da escala:

| Escala | Posições em duas oitavas |
|---|---:|
| Major / Ionian | 15 |
| Minor / Aeolian | 15 |
| Harmonic Minor | 15 |
| Minor Pentatonic | 11 |
| Major Pentatonic | 11 |
| Whole Tone | 13 |
| Chromatic | 25 |

A escala cromática percorre todos os semitons. Nas demais escalas, o sensor faz snapping para as notas correspondentes à escala. A distância útil é distribuída entre as posições de cada escala; portanto, a distância física por nota não é necessariamente igual em semitons.

## Números CC

Cada sensor possui seu próprio número de Control Change, inicialmente CC11. Os botões D6/D7 alteram o número do sensor selecionado, com retorno circular entre 0 e 127. Todos os números permanecem disponíveis intencionalmente, inclusive os reservados ou associados a funções especiais. Isso oferece flexibilidade para usuários avançados, mas alguns números podem executar ações globais no sintetizador ou na DAW, como All Sound Off, All Notes Off, Reset All Controllers ou Sustain.

Ao mudar o número CC ou o canal, o firmware envia valor zero para o controlador/canal anteriormente utilizado, para tentar liberar o valor mantido.

## Pinagem do Arduino Leonardo

### Sensores ultrassônicos

| Componente | Terminal | Leonardo |
|---|---|---|
| HC-SR04 1 | VCC | 5V |
| HC-SR04 1 | GND | GND |
| HC-SR04 1 | TRIG | A0 |
| HC-SR04 1 | ECHO | A1 |
| HC-SR04 2 | VCC | 5V |
| HC-SR04 2 | GND | GND |
| HC-SR04 2 | TRIG | A2 |
| HC-SR04 2 | ECHO | A3 |

Os dois sensores compartilham 5V e GND. Os disparos são alternados para reduzir interferência ultrassônica.

### OLED SSD1306 I2C

| OLED | Leonardo |
|---|---|
| VCC | 5V ou 3.3V conforme o módulo |
| GND | GND |
| SDA | D2 / SDA |
| SCL | D3 / SCL |

Confirme se o módulo OLED aceita 5V antes de alimentá-lo pelo pino 5V.

### Botões principais

Cada botão deve ligar seu pino ao **GND** quando pressionado. O firmware usa `INPUT_PULLUP`, portanto não são necessários resistores externos.

| Botão | Pino | Função |
|---|---|---|
| 1 | D4 | Alternar sensor selecionado (1 / 2) |
| 2 | D5 | Ciclar modo do sensor selecionado (NOTES / VELOCITY / CC / PITCH) |
| 3 | D6 | CC -1 do sensor selecionado |
| 4 | D7 | CC +1 do sensor selecionado |
| 5 | D8 | Oitava -1 do sensor selecionado |
| 6 | D9 | Oitava +1 do sensor selecionado |
| 7 | D10 | Canal MIDI -1 do sensor selecionado |
| 8 | D11 | Canal MIDI +1 do sensor selecionado |
| 9 | D12 | Nota-base global -1 (C → B, com ciclo) |
| 10 | D13 | Nota-base global +1 (C → C# → ... → B → C) |
| 11 | A4 | Próximo modo de escala global |
| 12 | A5 | Sustain global (pressionado = ligado; solto = desligado) |

O botão Sustain envia CC64 = 127 para cada canal MIDI distinto configurado nos sensores. Ao soltar, envia CC64 = 0 para todos os canais que permanecem sob Sustain. Se o canal de um sensor for alterado enquanto o botão estiver pressionado, o firmware libera o canal antigo e ativa o novo.

### Botões opcionais CC ±10

| Botão opcional | Pino | Função |
|---|---|---|
| CC -10 | D0 / RX1 | CC -10 do sensor selecionado |
| CC +10 | D1 / TX1 | CC +10 do sensor selecionado |

Esses botões estão **desabilitados por padrão**. Para habilitá-los, altere no início de `Theremidi.ino`:

```cpp
#define ENABLE_CC_STEP10_BUTTONS 0
```

para `1`. Não utilize `Serial1` enquanto D0/D1 estiverem funcionando como botões.

## Display OLED

- A tela de configuração mostra o sensor selecionado e seus parâmetros individuais de oitava/canal.
- Em NOTES, mostra a nota tocada, a escala global, a oitava individual e o canal de saída.
- Em PITCH, mostra a nota-base, o valor Pitch Bend e o canal.
- Em VELOCITY, mostra o valor global retido.
- Em CC, mostra o valor, o número CC e o canal do sensor selecionado.
- Ao pressionar um botão de configuração, a tela de configuração aparece por aproximadamente 800 ms. A tela de nota retorna depois que esse período termina.

## Faixas, filtro e calibração inicial

Os valores iniciais permanecem ajustáveis em `Theremidi.ino`:

- Distância útil: 8–60 cm.
- Timeout do eco: 6000 µs.
- Intervalo entre disparos alternados: 60 ms.
- Filtro EMA normal: alpha 0.35.
- Filtro EMA para movimentos maiores: alpha 0.70 quando a leitura muda pelo menos 3 cm.
- Histerese na troca de posição da escala: 0.15 da distância entre posições adjacentes.
- Duas leituras inválidas para liberar o sensor.

A histerese reduz oscilações de nota quando a mão fica próxima da fronteira entre posições. O filtro adaptativo suaviza pequenas variações e responde mais rapidamente a deslocamentos maiores. Ajuste esses valores depois de testar os sensores na montagem física.

## Bibliotecas e compilação

Para compilar pela Arduino IDE, instale as bibliotecas compatíveis:

- MIDIUSB
- Adafruit SSD1306
- Adafruit GFX Library

Selecione **Arduino Leonardo** como placa. O firmware utiliza o USB nativo do ATmega32U4 para MIDI.

## Histórico desta revisão

- Oitava independente por sensor.
- Canal MIDI independente por sensor, para roteamento separado em DAWs como REAPER.
- Quarto modo de sensor: PITCH, com nota-base e Pitch Bend contínuo.
- Configuração RPN de sensibilidade Pitch Bend em ±12 semitons.
- Nova escala Chromatic.
- Todas as escalas de snapping normalizadas para uma extensão de duas oitavas, com quantidade de posições específica por escala.
- Filtro ultrassônico adaptativo e histerese para reduzir oscilações de notas.
- Sustain global aplicado a todos os canais distintos dos sensores.
- Números CC 0–127 mantidos disponíveis intencionalmente.
