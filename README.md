<div align="center">

# 🌱 AgroFuzzy

### Sistema inteligente de irrigação agrícola baseado em Lógica Fuzzy

<p>
  <img src="https://img.shields.io/badge/ESP32-E7352C?style=for-the-badge&logo=espressif&logoColor=white">
  <img src="https://img.shields.io/badge/C%2B%2B-00599C?style=for-the-badge&logo=cplusplus&logoColor=white">
  <img src="https://img.shields.io/badge/Arduino_IDE-00979D?style=for-the-badge&logo=arduino&logoColor=white">
  <img src="https://img.shields.io/badge/Wokwi-000000?style=for-the-badge&logo=wokwi&logoColor=white">
  <img src="https://img.shields.io/badge/Fuzzy_Logic-6A1B9A?style=for-the-badge">
</p>

<p>
  <strong>🌾 Agricultura • 🤖 Sistemas Inteligentes • 📐 Matemática • 💻 Sistemas Embarcados</strong>
</p>

<br>

<img src="https://capsule-render.vercel.app/api?type=waving&color=0:2E7D32,100:81C784&height=120&section=header&text=AgroFuzzy&fontSize=42&fontColor=ffffff&animation=fadeIn&fontAlignY=35"/>

</div>

---

## 🌱 Sobre o projeto

O **AgroFuzzy** é um projeto de pesquisa e desenvolvimento de um sistema inteligente de irrigação agrícola baseado em **Lógica Fuzzy**, desenvolvido com um microcontrolador **ESP32**.

A proposta é desenvolver um controlador capaz de analisar variáveis ambientais, principalmente:

* 💧 Umidade do solo
* 🌡️ Temperatura
* 🌱 Condições relacionadas ao cultivo

e, a partir dessas informações, determinar **quanto irrigar**, em vez de utilizar apenas regras rígidas do tipo:

> "Se a umidade estiver abaixo de X%, ligue a bomba."

A utilização da Lógica Fuzzy permite representar situações intermediárias e trabalhar com diferentes graus de pertencimento.

O projeto tem como aplicação inicial o **manejo da irrigação da cultura da soja**, buscando relacionar conceitos de **matemática, computação, eletrônica e agronomia**.

---

## 🎯 Objetivo

Desenvolver e avaliar um sistema embarcado capaz de utilizar **inferência fuzzy** para auxiliar na tomada de decisões relacionadas à irrigação agrícola.

### Objetivos específicos

* Desenvolver um sistema de aquisição de dados ambientais;
* Medir a umidade do solo;
* Medir a temperatura;
* Modelar as variáveis utilizando conjuntos fuzzy;
* Desenvolver um sistema de inferência Mamdani;
* Utilizar defuzzificação pelo método do centróide;
* Controlar automaticamente o acionamento da irrigação;
* Simular o sistema utilizando o **Wokwi**;
* Implementar o controlador no **ESP32**;
* Avaliar experimentalmente o comportamento do sistema;
* Relacionar os resultados às condições recomendadas para a cultura da soja.

---

## 🧠 Como funciona?

O sistema pode ser representado pela seguinte arquitetura:

```text
                  🌡️ Temperatura
                       │
                       ▼
                 ┌─────────────┐
                 │             │
💧 Umidade ─────►│   ESP32     │
                 │             │
                 └──────┬──────┘
                        │
                        ▼
                ┌───────────────┐
                │ Fuzzyficador  │
                └───────┬───────┘
                        │
                        ▼
                ┌───────────────┐
                │ Base de regras│
                │    Mamdani    │
                └───────┬───────┘
                        │
                        ▼
                ┌───────────────┐
                │ Defuzzificação│
                │   Centrôide   │
                └───────┬───────┘
                        │
                        ▼
                  💧 Irrigação
```

O controlador recebe os dados dos sensores, transforma os valores numéricos em **graus de pertinência**, aplica as regras fuzzy e, posteriormente, transforma o resultado novamente em um valor numérico.

---

# 📐 Modelo matemático

O controlador é baseado em um sistema de inferência **Mamdani**.

De forma simplificada:

$$
(U,T) \longrightarrow F(U,T) \longrightarrow V
$$

onde:

* \(U\) = umidade do solo;
* \(T\) = temperatura;
* \(F\) = sistema de inferência fuzzy;
* \(V\) = quantidade de água determinada pelo controlador.

A ideia é que o resultado final possa ser relacionado à vazão real do sistema:

$$
t = \frac{V}{Q}
$$

onde:

* \(t\) = tempo de acionamento;
* \(V\) = volume de água desejado;
* \(Q\) = vazão da bomba.

Essa abordagem evita que o tempo de acionamento seja simplesmente um valor arbitrário.

---

## 🔬 Fuzzyficação

As variáveis de entrada são representadas por conjuntos fuzzy.

### 💧 Umidade

Exemplo de conjuntos utilizados durante o desenvolvimento:

```text
Baixa ───── Média ───── Adequada ───── Alta
  0           35          55  65       85       100 %
```

As funções de pertinência são construídas utilizando funções triangulares e trapezoidais.

Por exemplo:

$$
\mu(x)=
\begin{cases}
0 & x<a \\
\dfrac{x-a}{b-a} & a\leq x<b \\
1 & b\leq x\leq c \\
\dfrac{d-x}{d-c} & c<x\leq d \\
0 & x>d
\end{cases}
$$

Os parâmetros das funções devem ser posteriormente **calibrados e validados experimentalmente**, levando em consideração o sensor utilizado e as condições do solo.

---

## 🌡️ Temperatura

A temperatura também é dividida em conjuntos fuzzy:

```text
Baixa ───────── Adequada ───────── Alta
  │                 │                │
 -10               20               35       60 °C
```

As faixas utilizadas no protótipo são parâmetros de desenvolvimento e não devem ser interpretadas como limites universais para a cultura da soja.

---

# ⚙️ Regras fuzzy

A base de regras procura combinar as condições ambientais.

Exemplos:

| Umidade  | Temperatura | Decisão                 |
| -------- | ----------- | ----------------------- |
| Baixa    | Baixa       | Irrigação moderada      |
| Baixa    | Adequada    | Irrigação elevada       |
| Baixa    | Alta        | Irrigação elevada       |
| Média    | Baixa       | Irrigação curta         |
| Média    | Adequada    | Irrigação média         |
| Média    | Alta        | Irrigação média/elevada |
| Adequada | Baixa       | Irrigação mínima        |
| Adequada | Adequada    | Irrigação mínima        |
| Alta     | Qualquer    | Sem irrigação           |

A lógica não considera a temperatura isoladamente. A **umidade do solo possui papel central**, enquanto a temperatura atua como uma variável complementar na decisão.

---

# 🧮 Inferência Mamdani

Para cada regra, é calculado o grau de ativação.

Para uma regra do tipo:

$$
U=\text{Baixa} \land T=\text{Alta}
$$

utiliza-se:

$$
\alpha=\min(\mu_U,\mu_T)
$$

As regras são posteriormente agregadas utilizando o operador máximo:

$$
\mu_{\text{saída}}(z)
=
\max_i
\left[
\min(\alpha_i,\mu_i(z))
\right]
$$

---

# 📊 Defuzzificação

Após a agregação das regras, é necessário transformar o conjunto fuzzy resultante em um valor numérico.

O projeto utiliza o **método do centróide**:

$$
z^*=
\frac{
\int z\mu(z)\,dz
}{
\int \mu(z)\,dz
}
$$

Na implementação embarcada, o cálculo pode ser aproximado numericamente:

$$
z^*=
\frac{
\sum_i z_i\mu(z_i)
}{
\sum_i\mu(z_i)
}
$$

Esse valor pode representar a quantidade de água desejada ou outra variável intermediária utilizada pelo controlador.

---

# 🔌 Hardware

### Componentes principais

| Componente                       | Função                                       |
| -------------------------------- | -------------------------------------------- |
| **ESP32**                        | Processamento e controle                     |
| **Sensor capacitivo de umidade** | Medição da umidade do solo                   |
| **DS18B20**                      | Medição da temperatura                       |
| **Sistema de acionamento**       | Controle da bomba                            |
| **Bomba d'água**                 | Irrigação                                    |
| **Mangueiras**                   | Transporte da água                           |
| **Distribuidor**                 | Divisão da água entre os pontos de irrigação |

### Arquitetura eletrônica

```text
       ┌─────────────────┐
       │ Sensor umidade  │
       └────────┬────────┘
                │
                ▼
       ┌─────────────────┐
       │                 │
       │      ESP32      │
       │                 │
       └───────┬─────────┘
               │
               ▼
       ┌─────────────────┐
       │ Acionamento da  │
       │     bomba       │
       └───────┬─────────┘
               │
               ▼
          💧 Irrigação
```

> ⚠️ **Segurança:** a bomba utilizada no protótipo possui alimentação em corrente alternada. A parte ligada à rede elétrica deve permanecer isolada da eletrônica de baixa tensão e ser montada/manuseada por pessoa qualificada. O ESP32 não deve ser conectado diretamente à rede elétrica.

---

# 💻 Software

O projeto utiliza:

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-00599C?style=flat-square&logo=cplusplus&logoColor=white">
  <img src="https://img.shields.io/badge/Arduino_IDE-00979D?style=flat-square&logo=arduino&logoColor=white">
  <img src="https://img.shields.io/badge/Wokwi-000000?style=flat-square&logo=wokwi&logoColor=white">
</p>

### Linguagem

**C++**

Utilizada para implementação do sistema embarcado no ESP32.

### Bibliotecas

```cpp
#include <OneWire.h>
#include <DallasTemperature.h>
```

### Simulação

O **Wokwi** é utilizado para testar o comportamento do sistema antes da implementação física.

---

# 🧪 Simulação

Durante o desenvolvimento, o sistema recebe valores simulados de temperatura e umidade e calcula a resposta do controlador.

Exemplo:

```text
Umidade      = 77,66 %
Temperatura  = 19,4 °C

        ↓

Fuzzyficação

        ↓

Inferência Mamdani

        ↓

Agregação

        ↓

Defuzzificação

        ↓

Decisão de irrigação
```

Os valores obtidos na simulação são utilizados para validar a implementação matemática antes dos testes físicos.

---

# 🌾 Aplicação na soja

A cultura escolhida inicialmente para o projeto é a **soja**.

A escolha permite estudar a relação entre:

* disponibilidade de água;
* temperatura;
* desenvolvimento da cultura;
* necessidade hídrica;
* manejo da irrigação.

As condições agronômicas utilizadas no sistema devem ser fundamentadas em literatura técnica e científica, especialmente materiais de instituições de pesquisa agrícola, e posteriormente ajustadas por meio de experimentação.

> **Importante:** os valores utilizados nas funções fuzzy são parâmetros do modelo e precisam ser validados experimentalmente. A porcentagem fornecida por um sensor de umidade não deve ser automaticamente interpretada como porcentagem volumétrica de água no solo.

---

# 🔬 Metodologia experimental

Uma etapa importante do projeto será comparar o comportamento do controlador em diferentes condições.

### Variáveis de entrada

* Umidade do solo;
* Temperatura;
* Futuramente, estágio fenológico da cultura.

### Variável de saída

Inicialmente:

$$
t_{\text{irrigação}}
$$

Posteriormente:

$$
V_{\text{água}}
$$

e:

$$
t=\frac{V}{Q}
$$

### Possíveis avaliações

* estabilidade das leituras;
* resposta do controlador;
* volume de água aplicado;
* tempo de acionamento;
* comportamento em diferentes condições de umidade;
* comparação com um método baseado em limiar fixo;
* influência da temperatura na decisão;
* eficiência da distribuição de água.

---

# 🚀 Roadmap

* [x] Desenvolvimento inicial da ideia
* [x] Modelagem inicial das variáveis
* [x] Implementação da fuzzyficação
* [x] Implementação das regras Mamdani
* [x] Implementação da defuzzificação por centróide
* [x] Simulação inicial no Wokwi
* [ ] Calibração experimental do sensor de umidade
* [ ] Medição da vazão real da bomba
* [ ] Construção do sistema hidráulico
* [ ] Integração dos sensores físicos
* [ ] Testes experimentais
* [ ] Validação das funções de pertinência
* [ ] Modelagem da saída em volume de água
* [ ] Inclusão do estágio fenológico da soja
* [ ] Comparação com irrigação baseada em limiar
* [ ] Análise estatística dos resultados
* [ ] Desenvolvimento de interface para monitoramento
* [ ] Avaliação do sistema em diferentes condições ambientais

---

# 📁 Estrutura do projeto

```text
AgroFuzzy/
│
├── README.md
│
├── src/
│   └── AgroFuzzy.ino
│
├── simulation/
│   └── wokwi/
│
├── docs/
│   ├── metodologia/
│   ├── referencias/
│   └── resultados/
│
├── diagrams/
│   └── arquitetura.png
│
└── experiments/
    ├── dados/
    └── graficos/
```

---

# 📚 Referencial

O desenvolvimento do projeto utiliza conhecimentos provenientes de diferentes áreas:

### 🌾 Agronomia

Informações técnicas relacionadas ao cultivo da soja, disponibilidade hídrica, temperatura e manejo da irrigação.

### 📐 Matemática

* Lógica Fuzzy;
* Funções de pertinência;
* Inferência;
* Métodos de agregação;
* Defuzzificação;
* Métodos numéricos.

### 💻 Computação

* C++;
* Sistemas embarcados;
* Aquisição de dados;
* Algoritmos de controle;
* Simulação computacional.

### ⚡ Eletrônica

* ESP32;
* Sensores;
* Conversão de sinais;
* Controle de atuadores.

---

# 👨‍💻 Desenvolvimento

Projeto desenvolvido como iniciativa de pesquisa e desenvolvimento tecnológico, integrando **matemática, computação, eletrônica e agricultura**.

<div align="center">

### 🌱 AgroFuzzy

**Do dado ambiental à decisão inteligente de irrigação.**

<br>

<img src="https://capsule-render.vercel.app/api?type=waving&color=0:81C784,100:2E7D32&height=100&section=footer"/>

</div>
