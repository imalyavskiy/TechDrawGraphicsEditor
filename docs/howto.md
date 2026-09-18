# Работа с проектом «Технический рисунок / Technical Draw»

## 1. Назначение документа

Это практическая инструкция по первоначальной настройке, сборке, запуску, проверке и упаковке проекта на Windows. Команды рассчитаны на Qt 5.15.2 и MinGW 8.1.0 x64. Устройство исходного кода и формата `.drw` описано в [architecture.md](architecture.md), порядок этапов — в [PLAN.md](PLAN.md), обязательный Git Flow — в [../AGENTS.md](../AGENTS.md).

## 2. Необходимые инструменты

Установите:

1. Git.
2. CMake версии 3.16 или новее. Проверялись версии 3.29.2 и 4.3.2.
3. Ninja. Проверялась версия 1.12.0.
4. Qt 5.15.2 для MinGW x64 с модулями Core, Gui, Widgets и Svg. Сборка приложения использует закрытые заголовки Qt Gui для `QZipReader` и `QZipWriter`, поэтому комплект разработки должен содержать каталоги версионных закрытых заголовков.
5. MinGW 8.1.0 x64 из того же комплекта Qt. Смешивать компилятор и библиотеки Qt из разных комплектов нельзя.
6. Windows PowerShell 5.1. Он входит в поддерживаемые версии Windows и используется тестовым и упаковочными сценариями.
7. Штатный `%SystemRoot%\System32\iexpress.exe` для сборки EXE-инсталлятора.

Для изменения значка дополнительно нужны Python 3 и Pillow. Обычные сборка, запуск, тестирование и создание инсталлятора от Python не зависят.

Проверьте инструменты:

```bat
git --version
cmake --version
ninja --version
powershell.exe -NoProfile -Command "$PSVersionTable.PSVersion"
```

## 3. Первоначальная настройка

После получения чистой копии репозитория создайте локальный файл окружения:

```bat
copy scripts\windows\environment.example.bat scripts\windows\environment.bat
```

Откройте `scripts\windows\environment.bat` и задайте два пути без завершающего обратного слеша:

```bat
set "QT_ROOT=C:\Qt\5.15.2\mingw81_64"
set "MINGW_ROOT=C:\Qt\Tools\mingw810_64"
```

Файл `environment.bat` исключён через `.gitignore` и не должен попадать в коммиты. Вместо файла разрешено заранее определить одноимённые переменные среды. CMake и Ninja должны быть доступны через `PATH`.

Все публичные сценарии находятся в `scripts\windows`. Они вычисляют корень репозитория относительно собственного расположения через `%~dp0`, поэтому текущий каталог командной строки значения не имеет. Общий `common.bat` проверяет наличие `src`, `resources`, `docs`, `scripts\windows`, корневого `CMakeLists.txt`, Qt, MinGW, CMake и Ninja. При ошибке сценарий печатает причину и возвращает ненулевой код.

## 4. Быстрый цикл работы

Из корня репозитория:

```bat
scripts\windows\build-debug.bat
scripts\windows\run-debug.bat
scripts\windows\build-release.bat
scripts\windows\test.bat
```

Перед передачей изменений дополнительно соберите инсталлятор:

```bat
scripts\windows\build-installer.bat
```

Каждый сценарий можно вызвать абсолютным путём из другого каталога. Пути к проекту и инструментам могут содержать пробелы.

## 5. Сборка Debug

Команда:

```bat
scripts\windows\build-debug.bat
```

Сценарий вызывает внутренний `configure-build.bat Debug`, который:

1. проверяет корень проекта и инструменты через `common.bat`;
2. конфигурирует Ninja-проект в `build\cmake-debug`;
3. передаёт CMake пути к Qt и `g++.exe`;
4. собирает все доступные цели.

Результат: `build\cmake-debug\TechDraw.exe`. Это не переносимый комплект: для прямого запуска бинарнику нужны DLL и плагины из `QT_ROOT` и библиотеки MinGW.

Повторный запуск использует кэш CMake и выполняет только нужную пересборку. Если изменились компилятор, разрядность или комплект Qt, удалите именно каталог `build\cmake-debug` и запустите сценарий заново.

## 6. Сборка Release и переносимого комплекта

Команда:

```bat
scripts\windows\build-release.bat
```

Сначала `configure-build.bat Release` собирает `build\cmake-release\TechDraw.exe`. Затем сценарий заново создаёт `dist\TechDraw`, копирует EXE, Qt5Core/Gui/Widgets, библиотеки среды MinGW, плагины `qwindows`, `qoffscreen`, стиль Windows Vista и `qt.conf`.

Результаты:

- `build\cmake-release\TechDraw.exe` — бинарник сборки;
- `dist\TechDraw\TechDraw.exe` — точка запуска переносимого комплекта.

Для переноса на другой компьютер копируйте весь `dist\TechDraw`, сохраняя подкаталоги. Один EXE без DLL не запускается.

## 7. Прямые команды CMake

Командные файлы являются основным и проверяемым интерфейсом сборки. Для диагностики те же действия можно выполнить напрямую, предварительно задав `QT_ROOT` и `MINGW_ROOT`:

```bat
cmake -S . -B build\cmake-debug -G Ninja ^
  -DCMAKE_BUILD_TYPE=Debug ^
  -DCMAKE_PREFIX_PATH="%QT_ROOT%" ^
  -DCMAKE_CXX_COMPILER="%MINGW_ROOT%\bin\g++.exe"
cmake --build build\cmake-debug --parallel
```

Для Release замените `cmake-debug` на `cmake-release` и `Debug` на `Release`. Развёртывание библиотек выполняет только `build-release.bat`.

Windows-ресурс компилируется отдельной командой CMake без автоматически добавленных include-путей. Это обход несовместимого экранирования путей с пробелами внутри `windres` комплекта MinGW 8.1.0; не заменяйте эту команду обычным добавлением `.rc` в цель без повторной проверки пути с пробелами.

## 8. Запуск

Debug:

```bat
scripts\windows\run-debug.bat
```

Сценарий запускает `build\cmake-debug\TechDraw.exe`, добавляя Qt и MinGW в окружение только нового процесса.

Release:

```bat
scripts\windows\run-release.bat
```

Сценарий запускает `dist\TechDraw\TechDraw.exe` из переносимого каталога. Если нужный бинарник отсутствует, сценарий сообщает, какую сборку сначала выполнить.

## 9. Автоматические проверки

Сначала создайте переносимый Release-комплект, затем выполните:

```bat
scripts\windows\build-release.bat
scripts\windows\test.bat
```

`test.bat` через `test.ps1` последовательно запускает встроенную самопроверку:

- `offscreen` проверяет рисование, геометрию, историю и файлы без оконных диалогов;
- `windows` дополнительно проверяет настоящие диалоги Qt, поэтому окна могут кратковременно появляться.

Результаты записываются в `build\offscreen-results` и `build\windows-results`. В каждом каталоге находятся `result.txt`, `trace.txt`, тестовые `.drw` и PNG-снимки. Нулевой код завершения означает успех. Отсутствующий или не обновившийся `result.txt`, тайм-аут 20 секунд либо ненулевой код приложения считаются ошибкой.

Перед завершением этапа или крупного изменения проверяйте Debug отдельно: запустите `build-debug.bat`, затем передайте `build\cmake-debug\TechDraw.exe` параметры `-platform offscreen --self-test <каталог>` и `-platform windows --self-test <каталог>` в окружении Qt/MinGW.

## 10. Сборка инсталлятора

Команда:

```bat
scripts\windows\build-installer.bat
```

Сценарий:

1. повторно вызывает `build-release.bat`, поэтому пакет всегда соответствует текущим исходникам;
2. архивирует `dist\TechDraw` в `build\installer\payload.zip`;
3. формирует директиву IExpress;
4. создаёт `dist\installer\TechnicalDrawing-Setup.exe`.

Инсталлятор не требует прав администратора. Он устанавливает файлы в `%LOCALAPPDATA%\Programs\Technical Drawing`, создаёт ярлыки рабочего стола и меню «Пуск», а также пользовательскую запись удаления Windows. Установочный сценарий и архив находятся внутри EXE. Временные файлы сборки остаются в игнорируемом `build\installer` для диагностики.

Для изолированной проверки содержимого без ярлыков и записи удаления можно выполнить:

```powershell
build\installer\install-techdraw.ps1 `
  -InstallDirectory "$PWD\build\installer-test-install" `
  -SkipShellIntegration -Quiet
```

## 11. Генерация значка

Исходник: `resources\techdraw.svg`. Генерация использует временный CMake-проект с Qt Svg и затем Pillow:

```bat
py tools\generate_icon.py --qt-root "%QT_ROOT%" --compiler-root "%MINGW_ROOT%"
```

Результаты `resources\techdraw.png` и `resources\techdraw.ico` входят в репозиторий. После генерации проверьте оба файла визуально и убедитесь, что изменения ожидаемы.

## 12. Диагностика

| Сообщение или симптом | Причина и действие |
|---|---|
| `QT_ROOT is not set` или `MINGW_ROOT is not set` | Создайте `scripts\windows\environment.bat` по примеру либо задайте переменные среды. |
| `Qt5Config.cmake` не найден | `QT_ROOT` указывает не на корень комплекта Qt 5.15.2 MinGW. |
| `g++.exe` не найден | Исправьте `MINGW_ROOT`; компилятор должен соответствовать комплекту Qt. |
| CMake или Ninja не найден | Добавьте программу в `PATH` и откройте новое окно командной строки. |
| CMake сообщает о другом исходном каталоге | Удалите соответствующий `build\cmake-debug` или `build\cmake-release`; кэш был создан через другой путь или ссылку. |
| EXE не находит DLL | Запускайте через `run-debug.bat` либо используйте весь `dist\TechDraw`. |
| Самопроверка завершилась ошибкой | Откройте `result.txt` и `trace.txt` соответствующей платформы; повторите отдельно проблемный режим через `scripts\windows\test.ps1 -Platform ...`. |
| IExpress не найден | Проверьте `%SystemRoot%\System32\iexpress.exe`; он требуется только для инсталлятора. |
| Файл в `dist\TechDraw` занят | Закройте запущенный Release перед повторным `build-release.bat`. |

Все публичные сценарии возвращают `0` при успехе и ненулевой код при ошибке. Они прекращают работу после неудачной вложенной команды и не должны публиковать частично собранный результат как готовый.

## 13. Каталоги результатов

`build` и `dist` полностью производны и исключены из Git:

- `build\cmake-debug`, `build\cmake-release` — рабочие каталоги CMake;
- `build\offscreen-results`, `build\windows-results` — проверки;
- `build\installer` — промежуточные файлы упаковки;
- `dist\TechDraw` — переносимое приложение;
- `dist\installer` — готовый инсталлятор.

Удаляйте только конкретный каталог результата, который нужно пересоздать. Не используйте рекурсивное удаление с вычисленным или непроверенным пустым путём.

## 14. Git Flow проекта

Перед любым изменением проверьте ветку и рабочее дерево. `master` содержит стабильные выпуски, `develop` служит интеграционной веткой. Функции, рефакторинг, документация и исправления выполняются в отдельных `feature/<name>` от `develop`, затем вливаются в `develop`. Подготовка версии идёт в `release/<version>`, срочные исправления выпуска — в `hotfix/<name>`.

Функциональные изменения, рефакторинг, документация и комментарии к коду фиксируются раздельными логическими коммитами. Перед коммитом выполните подходящие сборки и проверки, затем `git diff --check`. Полные и обязательные правила находятся в [AGENTS.md](../AGENTS.md).
