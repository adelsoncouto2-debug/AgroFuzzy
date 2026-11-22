# AgroFuzzy Android

Projeto Android nativo que encapsula as telas do AgroFuzzy em uma `WebView`.

## Pré-requisitos

- Android Studio com SDK 35
- JDK 17

## Abrir e gerar o APK

1. Abra a pasta `android` no Android Studio.
2. Aguarde o Gradle sincronizar.
3. Execute o aplicativo em um celular ou use **Build > Build APK(s)**.
4. O APK será criado em `android/app/build/outputs/apk/debug/app-debug.apk`.

## Uso

1. Ligue o ESP8266 NodeMCU e conecte o celular à rede `IrrigacaoFuzzy`.
2. Abra o aplicativo.
3. O endereço padrão é `http://192.168.4.1`.
4. Use o botão de configurações para alterar o endereço, se necessário.
5. Selecione soja, milho, café ou cana.

O aplicativo envia `POST /api/cultura` e consulta `GET /api/status`.
