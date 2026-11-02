# Сборка под Windows

Клиент — это Qt 6 / C++ проект. Для сборки нужны MSVC, Qt и Conan. Ниже —
полный путь с нуля.

## 1. Что понадобится

| Компонент | Версия | Зачем |
| --- | --- | --- |
| Visual Studio 2022 Build Tools | 17.x | Компилятор MSVC и Windows SDK |
| CMake | 3.31 | Конфигурация и сборка |
| Python | 3.10+ | Conan и aqtinstall |
| Qt | 6.10.1, `win64_msvc2022_64` | UI, сеть, QML |
| Conan | 2.28.0 | Нативные зависимости (OpenSSL, OpenVPN, Xray, Wintun и т.д.) |
| Диск | ~25 ГБ | Qt, зависимости, артефакты |

## 2. Установка

Команды ниже выполняются в PowerShell. Установка Visual Studio требует прав
администратора, остальное ставится в пользовательский профиль.

### Visual Studio Build Tools

Минимальный набор — MSVC v143 и Windows SDK:

```powershell
winget install -e --id Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

Если на системном диске мало места, сам Visual Studio можно поставить на
другой диск:

```powershell
winget install -e --id Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --installPath <VSDir> --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

Windows SDK всё равно ставится в `%ProgramFiles(x86)%\Windows Kits\10`
(около 3–4 ГБ). После установки проверьте:

```powershell
Test-Path "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
```

### CMake, Python, Conan, aqtinstall

```powershell
py -m pip install --user "cmake==3.31.10" "conan==2.28.0" "aqtinstall==3.3.0"
```

Скрипт сборки автоматически находит пользовательские папки
`%APPDATA%\Python\Python*\Scripts` и добавляет их в `PATH`.

### Qt 6.10.1

```powershell
py -m aqt install-qt windows desktop 6.10.1 win64_msvc2022_64 `
    -O <QtDir> `
    -m qtremoteobjects qt5compat qtshadertools qthttpserver qtwebsockets
```

Если `download.qt.io` недоступен, добавьте зеркало:
`--base https://mirrors.ocf.berkeley.edu/qt/`.

Проверка:

```powershell
Test-Path <QtDir>\6.10.1\msvc2022_64\bin\qmake.exe
```

Должно быть `True`.

## 3. Настройка бренда и API

Перед сборкой откройте `brand/brand.cmake`:

```cmake
set(CLIENT_APPLICATION_NAME "MyVPN" ...)
set(CLIENT_PROVIDER_API_URL "https://vpn.example.com" ...)
```

`CLIENT_PROVIDER_API_URL` — база вашего API. Клиент запрашивает
`<url>/v1/servers`. Для локальной проверки не нужно пересобирать клиент:

```powershell
$env:PROVIDER_API_URL = "http://127.0.0.1:8080"
```

Переменная окружения имеет приоритет над CMake-значением.

## 4. Сборка

Из корня репозитория:

```powershell
powershell -ExecutionPolicy Bypass -File deploy\build_windows.ps1 -QtRoot <QtDir>\6.10.1
```

`-QtRoot` можно не указывать, если Qt лежит в `%USERPROFILE%\Qt` или в
стандартной папке Qt на системном диске: `build.bat` найдёт его сам. Для
Conan можно задать отдельный кэш:

```powershell
powershell -ExecutionPolicy Bypass -File deploy\build_windows.ps1 `
    -QtRoot <QtDir>\6.10.1 -ConanHome <ConanDir>
```

Или напрямую:

```powershell
$env:QT_ROOT_PATH = "<QtDir>\6.10.1"
cmd /c deploy\build.bat
```

Первая сборка долгая: Conan скачивает и при необходимости собирает OpenSSL,
OpenVPN, Xray и другие библиотеки. Заложите 30–90 минут и не прерывайте
процесс. Повторные сборки идут за минуты.

Готовые файлы появятся в `deploy\build\`. Установщик (необязательно):

```powershell
cmd /c deploy\build.bat --installer ifw
```

Для него дополнительно нужен Qt Installer Framework:

```powershell
py -m aqt install-tool windows desktop tools_ifw -O <QtDir>
```

### Сборка в GitHub Actions

Если не хочется ставить MSVC и Qt локально, в репозитории есть
`.github/workflows/fork-windows.yml`. Загрузите форк на GitHub, откройте
**Actions → Fork Windows build → Run workflow**. Готовый
`deploy/build/client/Release` появится в артефакте `MyVPN-windows-x64`.

Workflow не использует секреты Amnezia. Доступность всех зависимостей без
авторизации предстоит проверить первой сборкой. Параметр `api_url` задаёт
HTTPS-адрес API, вшитый в клиент. Workflow ещё не запускался.

## 5. Запуск и проверка провайдера

1. Запустите мок-сервер:

   ```powershell
   py tools\mock-provider\server.py --port 8080
   ```

2. Запустите клиент с `PROVIDER_API_URL=http://127.0.0.1:8080`.
3. Через 2 секунды клиент запросит список серверов и добавит их в общий
   список.
4. В логах приложения ищите строки `Provider:` и `Servers updated`.

Мок отдаёт конфиги с фиктивными ключами. Проверена выдача JSON по HTTP;
импорт и отображение в клиенте ещё требуют проверки после сборки.
Для реального подключения нужен настоящий сервер AWG.

Проверка мок-API:

```powershell
py -B -m unittest discover -s tools/mock-provider -p test_server.py -v
```

## 6. Частые проблемы

**`Could not find Qt6Config.cmake`** — неверный `QT_ROOT_PATH` или не
установлены дополнительные модули Qt. Проверьте, что в
`<QtDir>\6.10.1\msvc2022_64\lib\cmake` есть `Qt6RemoteObjects`.

**`conan: command not found`** — скрипт не нашёл пользовательский `Scripts`.
Запустите `py -m conan --version` и добавьте
`%APPDATA%\Python\Python311\Scripts` в `PATH` (или поправьте версию в
`deploy\build_windows.ps1`).

**`CMake 4.x` ломает старые рецепты** — используйте CMake 3.31:
`py -m pip install --user "cmake==3.31.10"`.

**Ошибка MSVC `cl.exe not found`** — Build Tools установлены без workload
C++. Переустановите компонент.

**Сборка падает на скачивании зависимостей** — Conan ходит на
`center.conan.io` и `artifactory.amnezia.org`. Если они недоступны,
настройте прокси: `conan remote list` и
переменных окружения `HTTPS_PROXY` и `HTTP_PROXY` (Conan 2).

**Не хватает места** — Qt и зависимости занимают ~20 ГБ. Держите проект,
Qt и кэш Conan на диске с запасом.
