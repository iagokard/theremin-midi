# Theremidi adaptado para 2 sensores HC-SR04

Esta cópia adapta o firmware do projeto original Theremidi para dois sensores ultrassônicos HC-SR04 e uma placa Arduino Leonardo/ATmega32U4. A lógica de escalas, oitavas, botões, display e USB MIDI foi mantida sempre que possível.

## Mapeamento dos sensores nesta versão

- **Sensor 1:** controla notas MIDI; quando o modo Pitch Bend é ativado pelos botões, controla Pitch Bend.
- **Sensor 2:** controla MIDI CC. O padrão continua sendo **CC7 (Volume)**, definido pela variável `CC` no arquivo `Theremidi.ino`.
- A velocity permanece fixa em **127**, como no código original.

> A ordem foi definida explicitamente para esta adaptação: o primeiro HC-SR04 é o sensor de notas e o segundo HC-SR04 é o sensor CC.

## Ligações no Arduino Leonardo

| HC-SR04 | Pino do módulo | Leonardo |
|---|---|---|
| Sensor 1 (notas/Pitch Bend) | VCC | 5V |
| | GND | GND |
| | TRIG | A0 (também D14) |
| | ECHO | A1 (também D15) |
| Sensor 2 (CC7) | VCC | 5V |
| | GND | GND |
| | TRIG | A2 (também D16) |
| | ECHO | A3 (também D17) |

O display OLED I2C original continua usando SDA/D2 e SCL/D3 no Leonardo. Os sete botões continuam nos pinos digitais D4 a D10, como na configuração original. Todos os módulos devem compartilhar o GND.

**Atenção:** o esquema elétrico e a imagem de circuito do repositório original representam os sensores Sharp infravermelhos. Eles não foram redesenhados nesta adaptação; para os HC-SR04, siga a tabela de ligações acima, não a ligação antiga dos sensores IR.

Os pinos A0–A3 do Leonardo também podem funcionar como entradas/saídas digitais, por isso são utilizados como TRIG/ECHO. Não conecte os sensores aos antigos pinos A0/A1 como entradas analógicas: nesta versão eles são usados como sinais digitais dos módulos.

## Parâmetros de leitura

No arquivo `Theremidi.ino`, procure por:

- `ULTRASONIC_MIN_DISTANCE_CM = 8.0`: limite próximo da área de toque.
- `ULTRASONIC_MAX_DISTANCE_CM = 60.0`: limite distante da área de toque.
- `ULTRASONIC_ECHO_TIMEOUT_US = 6000`: tempo máximo aguardando o ECHO (aprox. 103 cm).
- `ULTRASONIC_TRIGGER_INTERVAL_MS = 65`: intervalo entre disparos alternados. Cada sensor é lido aproximadamente a cada 130 ms.
- `ULTRASONIC_FILTER_ALPHA = 0.35`: suavização das leituras. Valores maiores respondem mais rápido; menores suavizam mais.

Distâncias fora da janela de 8–60 cm são tratadas como ausência da mão. Uma única leitura inválida é ignorada; após duas leituras inválidas consecutivas, a nota é desligada ou o CC é enviado como zero.

A aproximação da mão aumenta o índice da nota e o valor do CC. Para inverter essa direção, altere os pares de saída nos `fscale()` de `D_ULTRASONIC_SENSORS.ino`.

## Por que os sensores são alternados?

Os HC-SR04 precisam de um pulso de pelo menos 10 µs no TRIG. A duração do pulso ECHO é convertida para centímetros aproximadamente por `distância_cm = duração_us / 58`. A documentação do módulo recomenda um ciclo de medição superior a 60 ms para reduzir interferência entre disparo e eco. Esta implementação dispara um módulo, espera 65 ms e dispara o outro; assim, cada módulo é medido aproximadamente a cada 130 ms e os dois não disparam juntos.

A leitura usa `pulseIn()` com timeout de 6000 µs. Isso bloqueia o `loop()` por até cerca de 6 ms durante uma medição, mas é limitado para não prender o tratamento dos botões por longos períodos.

## Bibliotecas necessárias

Instale pelo Arduino IDE / Library Manager: 

- **MIDIUSB** (para USB MIDI no ATmega32U4).
- **Adafruit SSD1306**.
- **Adafruit GFX Library** (dependência do display SSD1306).

`Wire` faz parte do core Arduino. As bibliotecas `Thread` e `Average` do código original não são mais necessárias, porque o agendamento e a filtragem foram substituídos por uma rotina própria para os HC-SR04.

Se não for usar o OLED, remova/comente `#define USING_DISPLAY 1` no começo de `Theremidi.ino`; assim, a biblioteca SSD1306 deixa de ser necessária.

Para ver as distâncias no Monitor Serial durante a calibração, adicione `#define DEBUG 1` perto da definição `#define ATMEGA32U4 1` no começo de `Theremidi.ino`. Depois de calibrar, você pode remover/comentar `DEBUG`.

## Arch Linux: testar se o USB MIDI foi reconhecido

Depois de compilar e enviar o sketch para a placa:

```bash
sudo modprobe snd_seq_midi
aseqdump -l
```

Procure uma porta MIDI com nome semelhante a Arduino Leonardo. No Reaper, habilite essa entrada em **Options → Preferences → Audio → MIDI Devices**.

## Observações e limitações

- Esta adaptação foi escrita para HC-SR04 alimentado em 5 V e Arduino Leonardo também em lógica de 5 V.
- Não é necessário resistor divisor de tensão entre ECHO e o Leonardo de 5 V. Não use a mesma ligação diretamente em placas de lógica 3,3 V.
- A leitura sequencial reduz o risco de diafonia entre sensores, mas a resposta ainda depende do ambiente, do formato da mão e de superfícies refletoras.
- O HC-SR04 atualiza cada mão em aproximadamente 7,7 Hz (uma leitura a cada 130 ms). A resposta pode parecer menos contínua que os sensores Sharp analógicos do projeto original.
- Os valores de 8–60 cm e o filtro alpha de 0,35 são pontos de partida; ajuste-os depois de testar fisicamente.
- Não foi possível compilar/verificar este firmware contra a placa física nesta etapa. Antes da apresentação, valide a pinagem, a leitura serial (com `DEBUG` habilitado) e os eventos MIDI no seu Leonardo.

## Crédito do projeto original

Esta adaptação deriva do repositório Theremidi, de Gustavo Silveira: https://github.com/silveirago/Theremidi. Mantenha os créditos e consulte a licença/termos do repositório original ao redistribuir ou apresentar o projeto.
