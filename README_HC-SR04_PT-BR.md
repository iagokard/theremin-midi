# Theremidi HC-SR04 — Arduino Leonardo (ATmega32U4)

Esta versão parte da adaptação anterior para dois sensores HC-SR04 e acrescenta seleção independente de modo por sensor, velocity global, seleção de CC por sensor e novos controles.

## Modos independentes dos sensores

O botão **D4** alterna o sensor selecionado (Sensor 1 / Sensor 2). O botão **D5** percorre os modos do sensor selecionado nesta ordem:

1. **NOTES** — o sensor gera notas MIDI de acordo com a escala, a nota base e a oitava.
2. **VELOCITY** — o sensor altera a velocity global (1–127), utilizada em todos os próximos eventos Note On.
3. **CC** — o sensor envia um MIDI Control Change com valor de 0–127.
4. Ao pressionar novamente, volta para **NOTES**.

Os modos são armazenados separadamente. A configuração inicial é Sensor 1 = NOTES e Sensor 2 = CC. Ambos os sensores podem operar simultaneamente conforme os seus modos. Os parâmetros musicais (oitava, canal, nota base e escala) são compartilhados por todos os sensores configurados em NOTES.

Exemplo para usar o Sensor 1 em notas e o Sensor 2 em velocity: pressione D4 uma vez para selecionar o Sensor 2; pressione D5 duas vezes (CC → NOTES → VELOCITY). O Sensor 1 continua em NOTES.

**Velocity:** o valor é global e fica retido quando o sensor atribuído a VELOCITY deixa de receber uma leitura válida. A alteração só afeta notas iniciadas depois; não altera a velocity de notas que já estão tocando. Se os dois sensores forem configurados como VELOCITY, ambos escreverão no mesmo valor global; como as medições alternam, a última leitura válida prevalece. Recomenda-se utilizar apenas um sensor nesse modo.

**CC:** cada sensor possui seu próprio número de controlador CC. Os botões CC +/- alteram o número do sensor selecionado. O valor é zerado quando a mão sai da área válida, após o filtro de duas leituras inválidas. Se os dois sensores estiverem em CC e usarem o mesmo número no mesmo canal, o último valor recebido prevalece; configure números diferentes para controlar parâmetros distintos.

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

Confirme se o módulo OLED aceita 5V antes de alimentar pelo pino 5V.

### Botões principais

Esta versão utiliza **12 botões principais** (incluindo o Sustain). Como o kit informado inicialmente contém 10 botões táteis, será necessário adquirir mais 2 botões para montar todas as funções simultaneamente. Os dois botões CC ±10 são adicionais e opcionais.

Cada botão deve ligar seu pino ao **GND** quando pressionado. O firmware usa `INPUT_PULLUP`, portanto não são necessários resistores externos. Posicione os botões táteis atravessando a canaleta central da protoboard.

| Botão | Pino | Função |
|---|---|---|
| 1 | D4 | Alternar sensor selecionado (1 / 2) |
| 2 | D5 | Ciclar modo do sensor selecionado (NOTES / VELOCITY / CC) |
| 3 | D6 | CC -1 do sensor selecionado |
| 4 | D7 | CC +1 do sensor selecionado |
| 5 | D8 | Oitava -1 |
| 6 | D9 | Oitava +1 |
| 7 | D10 | Canal MIDI -1 |
| 8 | D11 | Canal MIDI +1 |
| 9 | D12 | Nota base -1 (C → B, com ciclo) |
| 10 | D13 | Nota base +1 (C → C# → ... → B → C) |
| 11 | A4 | Próximo modo de escala |
| 12 | A5 | Sustain (pressionado = ligado; solto = desligado) |

A oitava, canal MIDI, nota base e modo de escala são parâmetros musicais globais, compartilhados pelos sensores configurados em NOTES.

### Botões opcionais CC ±10

Dois pinos ficam reservados para alteração mais rápida do número CC:

| Botão opcional | Pino | Função |
|---|---|---|
| CC -10 | D0 / RX1 | CC -10 do sensor selecionado |
| CC +10 | D1 / TX1 | CC +10 do sensor selecionado |

Esses botões estão **desabilitados por padrão**. Para habilitá-los, altere no início de `Theremidi.ino`:

```cpp
#define ENABLE_CC_STEP10_BUTTONS 0
```

para:

```cpp
#define ENABLE_CC_STEP10_BUTTONS 1
```

Com a opção habilitada, conecte cada botão entre o pino indicado e GND. Não utilize `Serial1` enquanto D0/D1 estiverem funcionando como botões.

## Controles musicais

- **Oitava - / + (D8/D9):** altera o deslocamento de oitava entre -2 e +2.
- **Canal MIDI - / + (D10/D11):** seleciona os canais MIDI 1 a 16.
- **Nota base - / + (D12/D13):** percorre C, C#, D, D#, E, F, F#, G, G#, A, A# e B, com retorno circular.
- **Modo de escala (A4):** percorre Major, Minor (Aeolian), HarmMin, MinPent, MajPent e Whole.
- **Sustain (A5):** envia CC64 = 127 enquanto pressionado e CC64 = 0 ao soltar.

A nota base inicial é C, a escala inicial é Major, o deslocamento de oitava é zero e o canal MIDI inicial é 1.

## Display OLED

- No modo NOTES, mostra a nota enquanto está tocando, com a escala/nota base e o canal. Depois que a nota termina, espera aproximadamente 250 ms e volta à tela de configuração.
- Ao pressionar um botão de configuração, a tela de configuração aparece por aproximadamente 800 ms. Se uma nota ainda estiver ativa, a tela volta a mostrar essa nota depois desse intervalo.
- No modo VELOCITY, mostra o valor global retido (1–127).
- No modo CC, mostra o valor enviado em tamanho maior e o número CC correspondente.

## Faixas e calibração inicial

Os valores iniciais permanecem ajustáveis em `Theremidi.ino`:

- Distância útil: 8–60 cm.
- Timeout do eco: 6000 µs.
- Intervalo entre disparos alternados: 65 ms.
- Filtro: alpha 0.35.
- Duas leituras inválidas para liberar o sensor.

Cada sensor é medido aproximadamente a cada 130 ms. Ajuste os limites e o filtro depois de testar na montagem física.

## Bibliotecas

Para compilar pela Arduino IDE, instale as bibliotecas compatíveis:

- MIDIUSB
- Adafruit SSD1306
- Adafruit GFX Library

Selecione **Arduino Leonardo** como placa. O firmware utiliza o USB nativo do ATmega32U4 para MIDI.

## Mudanças em relação à versão HC-SR04 anterior

- Os modos NOTES / VELOCITY / CC são independentes por sensor.
- Sensor 1 começa em NOTES e Sensor 2 começa em CC.
- Velocity global com retenção do último valor válido.
- Número CC independente por sensor, começando em CC11.
- Nota base cromática C–B e seis modos de escala.
- Novos botões e pinagem documentada.
- Sustain preservado.
- Botões opcionais CC ±10 reservados em D0/D1 e desabilitados por padrão.
- O antigo atalho combinado de Pitch Bend foi removido, pois o seletor agora percorre os três modos solicitados.
