# Prixum Launcher

Prixum Launcher — форк [Prism Launcher](https://github.com/PrismLauncher/PrismLauncher) с новым дизайном **Nova**,
полностью редактируемыми темами и **масштабированием рендера** (игра рендерится в меньшем разрешении и растягивается
до размера окна — с размытием или без). Весь функционал Prism сохранён.

## Что изменилось

- **Главное окно.** Боковая панель навигации, поиск по экземплярам (`Ctrl+F`), сортировка, карточки экземпляров с
  подсветкой и значками статуса, панель выбранного экземпляра с большой кнопкой «Запустить», строка новостей.
  Всё, что было на старых панелях инструментов, осталось на месте: те же действия, меню и горячие клавиши.
  В меню «Вид» можно включить компактную боковую панель, скрыть панель экземпляра, строку новостей и строку состояния;
  классическое меню включается в «Настройки → Главное».
- **Дизайн Nova** во всех окнах: настройки, окно экземпляра, мастер создания, диалоги. Встроенные темы:
  Nova Prixum (по умолчанию), Nova Dark, Nova Light, Nova Midnight, Nova Grass Block, Nova AMOLED, Nova Sakura.
  Классические темы Prism никуда не делись — они в меню «Темы» под разделителем.
- **Иконки Nova.** Монохромный набор, который перекрашивается под цвета темы (страницы настроек, окно экземпляра,
  меню). Выбирается в «Настройки → Внешний вид → Иконки». Логотипы сервисов (CurseForge, Modrinth и т. д.) остаются цветными.
- **Редактор тем** — «Темы → Редактор тем...» или кнопка в «Настройки → Внешний вид».
- **Масштабирование рендера** — «Настройки → Minecraft → Масштабирование», для отдельного экземпляра — в его параметрах.
- **Название и логотип.** Prixum Launcher, бинарник `prixumlauncher`, логотип Prism в розовых тонах.
- **Русский перевод** всех новых строк (поверх обычного перевода Prism).

## Сборка

Зависимости (Arch / CachyOS):

```bash
sudo pacman -S --needed base-devel cmake ninja extra-cmake-modules jdk-openjdk qt6-base qt6-svg qt6-imageformats qt6-networkauth qt6-5compat tomlplusplus cmark qrencode zlib libarchive gamescope
```

```bash
git submodule update --init libraries/libnbtplusplus
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DLauncher_ENABLE_JAVA_DOWNLOADER=ON -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build-release
cmake --install build-release
```

Java-часть лаунчера теперь собирается и новыми JDK (20+), в этом случае она компилируется под Java 8.

## Установщики и portable-версии

| Система | Установщик | Portable |
| --- | --- | --- |
| Windows (x64 MSVC, x64 MinGW, ARM64) | `PrixumLauncher-Windows-*-Setup-<версия>.exe` | `PrixumLauncher-Windows-*-Portable-<версия>.zip` |
| macOS (Intel + Apple Silicon) | `PrixumLauncher-macOS-<версия>.dmg` | `PrixumLauncher-macOS-Portable-<версия>.zip` |
| Linux (x86_64, aarch64) | `PrixumLauncher-Linux-<арх>.AppImage` | `PrixumLauncher-Linux-*-Portable-<версия>.tar.gz` |
| Arch / CachyOS | `prixumlauncher-<версия>-1-x86_64.pkg.tar.zst` | — |

Portable-версии хранят экземпляры, аккаунты и настройки **внутри своей папки** — её можно носить на флешке:
на Windows и Linux это делает файл `portable.txt` рядом с программой, на macOS — папка `UserData` рядом с
`Prixum Launcher.app`.

### Собрать всё для Windows, macOS и Linux (GitHub Actions)

Windows и macOS нельзя собрать на Linux, поэтому все установщики собирает GitHub:

1. Код лежит в [saxxumm/PrixumLauncher](https://github.com/saxxumm/PrixumLauncher), ветка `main`
   (`origin` — этот репозиторий, `upstream` — оригинальный Prism). Новые изменения отправляются обычным `git push`.

2. Подними версию в `CMakeLists.txt` (`Launcher_VERSION_MAJOR/MINOR/PATCH`), закоммить и поставь тег с **той же**
   версией, без `v` — сборка и публикация релиза запустятся сами (≈ 40–60 минут):

   ```bash
   git tag 12.0.1
   git push origin 12.0.1
   ```

   Встроенное обновление сравнивает тег релиза с версией программы, поэтому они должны совпадать.

3. Готовые файлы появятся на странице **Releases** репозитория (таблица выше + архив исходников).
   Пробную сборку без релиза можно запустить вручную: **Actions → Build → Run workflow**, файлы будут в артефактах.

Встроенный апдейтер (Windows, portable Linux, AppImage) проверяет обновления именно в релизах твоего репозитория,
а не у Prism. На macOS автообновление выключено.

Сборки не подписаны сертификатами: на Windows при первом запуске нажми «Подробнее → Выполнить в любом случае»,
на macOS — правый клик по приложению → «Открыть» (или `xattr -dr com.apple.quarantine "Prixum Launcher.app"`).
Подпись включается секретами репозитория так же, как у Prism (`APPLE_CODESIGN_*`, `AZURE_*`, `GPG_PRIVATE_KEY*`).

### Собрать Linux-пакеты локально

```bash
# пакет для Arch / CachyOS, сразу с установкой
cd packaging/arch && makepkg -si

# portable-архив из готовой сборки (кладётся в dist/)
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DLauncher_ENABLE_JAVA_DOWNLOADER=ON
cmake --build build-release
packaging/linux/make-portable.sh build-release
```

Локальный portable берёт Qt и библиотеки из системы сборки, поэтому работает на дистрибутивах не старее той,
где собран (у тебя это CachyOS — свежий glibc и процессор с AVX2). Portable из GitHub Actions собирается на
Ubuntu 24.04 и запускается почти везде.

## Данные

Prixum хранит данные отдельно от Prism: `~/.local/share/PrixumLauncher`. При первом запуске он найдёт
`~/.local/share/PrismLauncher` и предложит **скопировать** экземпляры, аккаунты, настройки, Java и т. д.
Prism при этом не трогается. Можно отказаться и указать свою папку ключом `-d <папка>`.

## Как редактировать дизайн

### Редактор тем

Открывается через «Темы → Редактор тем...». Каждое изменение сразу применяется ко всему лаунчеру.

- **Цвета** — основные цвета темы; раздел «Тонкая настройка» — производные цвета (наведение, выделение,
  полупрозрачный акцент и т. п.). Пустое поле = цвет вычисляется автоматически.
- **Форма и размеры** — скругления, отступы, размер шрифта, ширина боковой панели и панели экземпляра,
  размер плиток и значков экземпляров. Шрифт можно заменить на любой установленный.
- **Стили (QSS)** — свои правила [Qt Style Sheets](https://doc.qt.io/qt-6/stylesheet-reference.html) поверх
  базового стиля. «Вставить базовый стиль» копирует встроенный стиль целиком — с галкой
  «Полностью заменить базовый стиль» можно переделать вообще всё.
- «Сохранить» / «Сохранить как новую» — тема попадает в папку тем и сразу выбирается. «Экспорт...» сохраняет
  тему в любую папку, чтобы ей поделиться.

### Файлы темы

Тема — это папка в `~/.local/share/PrixumLauncher/themes/<имя>/`:

```text
themes/my-theme/
├── theme.json      цвета и размеры
├── style.qss       (необязательно) свои правила QSS
└── resources/      (необязательно) картинки, доступные в QSS как url(theme:файл.png)
```

`theme.json`:

```json
{
    "format": "nova",
    "name": "My Theme",
    "base": "dark",
    "colors": {
        "window": "#121014",
        "sidebar": "#0e0c10",
        "surface": "#1a171c",
        "surfaceAlt": "#211d24",
        "hover": "#2c2630",
        "border": "#322b36",
        "text": "#f1e9f0",
        "textMuted": "#a795a4",
        "link": "#ff8cc4",
        "accent": "#ff5fa2",
        "accentText": "#1a0610",
        "success": "#3ecf8e",
        "warning": "#f5b83d",
        "danger": "#ff5c6c",
        "tooltip": "#2a2430",
        "tooltipText": "#f1e9f0"
    },
    "metrics": {
        "radius": 12,
        "controlRadius": 8,
        "density": 6,
        "fontSize": 0,
        "fontFamily": "",
        "sidebarWidth": 240,
        "inspectorWidth": 300,
        "cardWidth": 112,
        "iconSize": 48
    },
    "replaceBaseQss": false
}
```

Любые ключи можно опустить — возьмутся значения по умолчанию (`"base": "light"` — от светлой темы).

**Лаунчер следит за файлами активной темы**: правите `theme.json` или `style.qss` в любом редакторе — после
сохранения тема перезагружается сама, перезапуск не нужен.

### Токены

В `style.qss` (и во встроенном стиле) можно писать `@имя` — оно заменяется значением из темы:

| Токены | Что это |
| --- | --- |
| `@window @sidebar @surface @surfaceAlt @hover @border` | фоны и границы |
| `@text @textMuted @link` | текст |
| `@accent @accentText` | акцент и текст на нём |
| `@success @warning @danger @tooltip @tooltipText` | статусы и подсказки |
| `@accentHover @accentPressed @accentSoft @pressed @borderStrong @textDisabled @surfaceAlt2 @scrollHandle @scrollHandleHover @knob @successSoft @warningSoft @dangerSoft` | производные цвета (можно переопределить в `colors`) |
| `@radius @controlRadius @radiusSmall @padY @padX` | скругления и отступы (уже с `px`) |
| `@fontSize @titleSize @subtitleSize @smallSize` | размеры шрифта (уже с `pt`) |
| `@sidebarWidth @inspectorWidth @cardWidth @iconSize` | размеры элементов главного окна |

Любой свой цвет из `colors` тоже становится токеном.

Пример `style.qss`:

```css
/* квадратная кнопка запуска с обводкой */
QToolButton#novaPlayButton {
    border-radius: 0px;
    border: 2px solid @accentHover;
}

/* другой фон боковой панели */
QFrame#novaSidebar {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @sidebar, stop:1 @accentSoft);
}
```

Полезные имена для селекторов главного окна: `#novaSidebar`, `#novaLibrary`, `#novaInspector`, `#novaNewsBar`,
`#novaPlayButton`, `#novaKillButton`, `#novaSearch`, `#novaSort`, `#novaPageTitle`, `#novaInstanceIcon`,
`#novaInstanceName`, `#novaStatusPill`; кнопки по ролям — `QPushButton[novaRole="nav"]`, `"primary"`, `"action"`,
`"danger"`, `"account"`, `"chip"`; окна настроек — `QListView#pageList`, `QLabel#pageHeader`.

### Исходники дизайна

| Что | Где |
| --- | --- |
| Базовый стиль (QSS с токенами) | `launcher/resources/nova/style.qss` |
| Встроенные темы | `launcher/resources/nova/presets/*.json` |
| Иконки (SVG, рисуются чёрным и перекрашиваются) | `launcher/resources/nova/icons/*.svg` |
| Компоновка главного окна (Qt Designer) | `launcher/ui/MainWindow.ui` |
| Карточки экземпляров и заголовки групп | `launcher/ui/instanceview/InstanceDelegate.cpp`, `VisualGroup.cpp` |
| Движок тем | `launcher/ui/themes/NovaTheme.*`, `NovaIcons.*` |
| Логотип | `program_info/org.prixumlauncher.PrixumLauncher*.svg` |

## Масштабирование рендера

Игра рендерится в уменьшенном разрешении, а итоговая картинка растягивается до размера окна или экрана.
Это даёт прирост FPS на слабом железе или «пиксельный» ретро-вид.

**Нужен [gamescope](https://github.com/ValveSoftware/gamescope)** (`sudo pacman -S gamescope`) — работает только в Linux.
Если gamescope не установлен, во вкладке будет подсказка, а игра запустится в обычном разрешении с предупреждением в журнале.

Настройки («Настройки → Minecraft → Масштабирование», для одного экземпляра — «Изменить → Параметры → Масштабирование»
с галкой переопределения):

- **Разрешение рендера** — процент от размера окна (кнопки 25/33/50/67/75 %). Ниже видно итоговое разрешение.
- **Фильтр масштабирования**
  - *Чёткие пиксели* (nearest) — **без размытия**;
  - *Сглаживание* (bilinear) — **с размытием**;
  - AMD FSR и NVIDIA NIS — с повышением резкости (регулируется ползунком «Резкость»).
- **Режим** — вписать с сохранением пропорций, целочисленно (идеальные пиксели), растянуть, заполнить с обрезкой.
- **Полноэкранный режим**, **захват курсора** (нужен, чтобы камера не крутилась), **доп. аргументы** gamescope.
- Превью: перетаскивайте разделитель, чтобы сравнить исходную картинку и результат.

Как это работает: лаунчер запускает игру так —
`gamescope -w <внутр.> -h <внутр.> -W <вывод> -H <вывод> -F <фильтр> -S <режим> [--sharpness N] [-f] [--force-grab-cursor] -- [обёртка] java ...`,
а окно Minecraft получает внутреннее разрешение. Команда пишется в журнал экземпляра. Кнопка «Остановить»
завершает gamescope вместе с игрой.

Советы: для ровных пикселей берите 50 %, 33 % или 25 % и режим «Целочисленно». Профилировщики (JProfiler,
VisualVM) при включённом масштабировании видят PID gamescope, а не Java.
