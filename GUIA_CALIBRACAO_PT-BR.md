# Theremidi v3.2 — guia de calibração e localização do código

Este guia corresponde à versão v3.2 entregue com o firmware. Os valores de distância são independentes por sensor; nota-base e escala continuam globais.

## 1. Distância mínima e máxima aceitas

Arquivo: `Theremidi/Theremidi.ino`, seção `SENSOR CONFIGURATION`.

```cpp
float ultrasonicMinDistanceCm[N_ULTRASONIC_SENSORS] = {8.0f, 8.0f};
float ultrasonicMaxDistanceCm[N_ULTRASONIC_SENSORS] = {60.0f, 60.0f};
```

- Primeiro valor = Sensor 1; segundo valor = Sensor 2.
- `ultrasonicMinDistanceCm` é o cutoff próximo (mão perto demais fica fora da zona).
- `ultrasonicMaxDistanceCm` é o cutoff distante (mão longe demais fica fora da zona).
- Os valores podem ser diferentes para cada sensor.
- `updateUltrasonicSensorState()` em `Theremidi/D_ULTRASONIC_SENSORS.ino` verifica se a leitura está dentro desses limites.
- Ao sair da faixa, o sensor só é liberado após `ULTRASONIC_INVALID_READINGS_TO_RELEASE` leituras inválidas consecutivas.

### Ajuste durante a execução com D0/D1

Os antigos botões opcionais CC -10/+10 agora são botões de calibração, habilitados por padrão:

| Pino | Ação no sensor selecionado |
|---|---|
| D0 | Distância máxima -1 cm |
| D1 | Distância máxima +1 cm |

O botão D4 seleciona Sensor 1 ou Sensor 2. D0/D1 ajustam apenas a distância máxima do sensor selecionado. A distância mínima permanece configurável nos arrays acima.

Limites dos botões:
- A distância máxima não pode ficar menor que `ultrasonicMinDistanceCm[sensor] + ULTRASONIC_MIN_PLAYING_SPAN_CM`.
- A distância máxima não passa de `ULTRASONIC_HARD_MAX_DISTANCE_CM` (100 cm), que foi limitada para ficar dentro do timeout de eco de 6000 µs.
- A mudança feita por D0/D1 é temporária: fica na RAM e volta aos valores dos arrays depois de reiniciar o Arduino. Depois de encontrar o valor ideal, copie o valor mostrado na tela para `ultrasonicMaxDistanceCm` no código.

O passo pode ser alterado em `DISTANCE_CALIBRATION_STEP_CM` (padrão 1,0 cm).

## 2. Quantidade de notas e resolução por distância

Arquivo: `Theremidi/Theremidi.ino`:

```cpp
const byte NOTE_RANGE_OCTAVES = 2;
```

Use `1` para distribuir uma oitava (12 semitons) pela faixa de distância, ou `2` para distribuir duas oitavas (24 semitons). O padrão é 2.

As tabelas `scaleIntervals` contêm até duas oitavas. `getActiveScaleNoteCount()` em `D_ULTRASONIC_SENSORS.ino` seleciona somente os graus que cabem na extensão configurada. Assim, não é preciso editar as tabelas para alternar entre uma e duas oitavas.

Quantidade de posições:

| Escala | 1 oitava | 2 oitavas |
|---|---:|---:|
| Major | 8 | 15 |
| Minor | 8 | 15 |
| Harmonic Minor | 8 | 15 |
| Minor Pentatonic | 6 | 11 |
| Major Pentatonic | 6 | 11 |
| Whole Tone | 7 | 13 |
| Chromatic | 13 | 25 |

A zona entre mínimo e máximo é normalizada e passada pela tabela `DISTANCE_CURVE_TABLE` (em `D_ULTRASONIC_SENSORS.ino`). Depois, `distanceToScalePosition()` divide a resposta transformada pelo número de intervalos da escala. Por isso, a distribuição é musical e não necessariamente linear em centímetros: a tabela de curva dá mais ou menos sensibilidade a determinadas partes do movimento.

Para mudar quais notas fazem parte de cada escala, edite `scaleIntervals`. Para mudar o número de oitavas sem alterar os intervalos, edite apenas `NOTE_RANGE_OCTAVES`.

## 3. Prioridade e conteúdo da OLED

Arquivo: `Theremidi/OLED_DISPLAY.ino`.

- `getCurrentlyPlayingSensor()` escolhe qual sensor deve aparecer como instrumento ativo.
- Se os dois sensores estiverem tocando, a OLED mostra o sensor selecionado por D4.
- Se somente um sensor estiver tocando, a OLED mostra esse sensor, mesmo que o outro esteja selecionado.
- Se nenhum estiver tocando, a OLED mostra por 250 ms a última nota liberada e depois volta à tela de configuração do sensor selecionado.
- Depois de pressionar um botão de configuração, a tela de configuração aparece por 800 ms (`DISPLAY_CONFIG_FLASH_MS`) e então retorna à tela de execução.

A tela de configuração também mostra os cutoffs `Near` e `Far` do sensor selecionado. Os botões D0/D1 atualizam esses valores na tela.

## 4. Botões D0/D1 de calibração

Arquivo: `Theremidi/Theremidi.ino`:

```cpp
#define ENABLE_DISTANCE_CALIBRATION_BUTTONS 1
```

O `1` habilita os botões D0/D1. O `0` os desabilita.

A ligação é a mesma dos demais botões: cada botão entre o pino digital e GND; o firmware usa `INPUT_PULLUP`. D0 é `DISTANCE_MAX_MINUS_PIN`; D1 é `DISTANCE_MAX_PLUS_PIN`.

A lógica está em `Theremidi/C_BUTTONS.ino`:
- `handleButtonPressed()` encaminha os eventos D0/D1.
- `adjustSelectedSensorMaximumDistance()` limita e atualiza o cutoff distante do sensor selecionado.
- `requestConfigDisplay()` faz a OLED apresentar a nova faixa.

D0 e D1 não mudam o número CC. Os botões D6/D7 continuam fazendo CC -1/+1.

## 5. Sustain toggle

O botão A5 agora é toggle:
- Um toque envia CC64 = 127 para todos os canais distintos configurados e ativa Sustain.
- Outro toque envia CC64 = 0 para esses canais e desativa Sustain.
- Soltar o botão não altera o estado.

A lógica está em `Theremidi/C_BUTTONS.ino`: `toggleGlobalSustain()`, `pressGlobalSustain()`, `releaseGlobalSustain()` e `syncGlobalSustainChannels()`.

A variável de estado é `sustainIsActive`, em `Theremidi/Theremidi.ino`. A OLED mostra `SUS:ON` ou `SUS:OFF` no canto superior direito, em todas as telas.

## 6. Filtro, histerese e velocidade de troca de nota

Os parâmetros estão no início de `Theremidi/Theremidi.ino`:

| Parâmetro | Padrão | O que controla |
|---|---:|---|
| `ULTRASONIC_TRIGGER_INTERVAL_MS` | 60 ms | Intervalo entre disparos alternados. Cada sensor é lido a cada aproximadamente 120 ms. |
| `ULTRASONIC_FILTER_ALPHA` | 0,35 | Suavização de pequenas variações. Menor = mais suave/lento; maior = mais rápido/menos suavizado. |
| `ULTRASONIC_FILTER_ALPHA_FAST` | 0,70 | Suavização usada em mudanças maiores. |
| `ULTRASONIC_FILTER_FAST_THRESHOLD_CM` | 3,0 cm | Diferença que faz o filtro passar para a resposta rápida. |
| `ULTRASONIC_INVALID_READINGS_TO_RELEASE` | 2 | Leituras fora da zona necessárias para encerrar a nota/atividade. |
| `NOTE_HYSTERESIS_Q8` | 166 | Histerese entre posições de escala, em ponto fixo Q8 (256 = 1 passo). |
| `BUTTON_DEBOUNCE_MS` | 25 ms | Debounce dos botões físicos; não controla a troca de notas. |

`NOTE_HYSTERESIS_Q8 = 166` equivale a aproximadamente 0,65 de um passo de escala. A mudança ocorre um pouco depois da fronteira matemática de 0,5, reduzindo o flicker. Aumentar o valor reduz oscilações, mas exige mover a mão mais longe para trocar de nota; diminuir torna a troca mais fácil, mas aumenta a chance de flicker.

O filtro EMA está em `updateUltrasonicSensorState()`; a decisão da posição e a histerese estão em `distanceToScalePosition()` no arquivo `D_ULTRASONIC_SENSORS.ino`.

O `PITCH_BEND_UPDATE_THRESHOLD` (padrão 4) é diferente: ele controla a diferença mínima no valor MIDI de 14 bits necessária para enviar outro Pitch Bend. Não controla o snapping de NOTES.

## 7. Pitch Bend: faixa, resposta e calibração

Arquivo: `Theremidi/Theremidi.ino`:

```cpp
const byte PITCH_BEND_RANGE_SEMITONES = 24;
```

Esse valor é aplicado para cada lado da nota-base: `-24` a `+24` semitons (quatro oitavas de extensão total). Para reduzir a faixa, use 12; para aumentar, use valores maiores, desde que o sintetizador receptor aceite essa sensibilidade.

A transformação da distância em valor MIDI de Pitch Bend (0–16383) está em `distanceToPitchBendValue()` no arquivo `D_ULTRASONIC_SENSORS.ino`. O envio e a filtragem da quantidade de mensagens estão em `processPitchBendSensor()`. A configuração da sensibilidade no sintetizador receptor via RPN 0,0 está em `configurePitchBendRange()` no arquivo `E_ATMEGA32U4.ino`.

O sintetizador/plug-in da DAW precisa aceitar o RPN enviado. Se ignorar RPN, configure manualmente a faixa de Pitch Bend do instrumento para o mesmo valor (por exemplo, ±24 semitons).

### Nota atual mostrada na OLED

O MIDI enviado não contém uma frequência acústica medida pelo Arduino: ele contém a nota-base (Note On) e o valor de Pitch Bend. A versão v3.2 calcula a nota temperada mais próxima a partir desses dois valores e mostra essa estimativa na OLED. A conta está em `calculatePitchBendDisplayNote()` (`D_ULTRASONIC_SENSORS.ino`), e a apresentação está em `drawPitchBendScreen()` (`OLED_DISPLAY.ino`).

É uma estimativa do MIDI enviado, não uma leitura do áudio real. Ela pode diferir se o instrumento receptor ignorar o RPN, usar outra sensibilidade, tiver transposição ou desafinação configurada.

## 8. Outros parâmetros úteis

- `SCALE_NOTE_COUNT` e `scaleIntervals`: número máximo de posições e intervalos de cada escala (Theremidi.ino).
- `rootPitchClass`: nota-base global (Theremidi.ino).
- `sensorOctaveIndex[]`: oitava de cada sensor (Theremidi.ino).
- `sensorMidiChannel[]`: canal MIDI de cada sensor (Theremidi.ino).
- `DISPLAY_CONFIG_FLASH_MS`: tempo que a tela de configuração fica na frente da tela de execução (Theremidi.ino).
- `DISPLAY_NOTE_RELEASE_DELAY_MS`: tempo que a última nota permanece visível depois de ser liberada (Theremidi.ino).
