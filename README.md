<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="program_info/org.prixumlauncher.PrixumLauncher.logo-darkmode.svg">
    <source media="(prefers-color-scheme: light)" srcset="program_info/org.prixumlauncher.PrixumLauncher.logo.svg">
    <img alt="Prixum Launcher" src="program_info/org.prixumlauncher.PrixumLauncher.logo.svg" width="360">
  </picture>
</p>

<p align="center">
  <b>Лаунчер Minecraft с новым дизайном, редактором тем и масштабированием рендера.</b><br>
  Несколько сборок игры, моды и модпаки, аккаунты Microsoft — всё в одном окне.
</p>

<p align="center">
  <a href="https://github.com/saxxumm/PrixumLauncher/releases/latest"><img alt="Версия" src="https://img.shields.io/github/v/release/saxxumm/PrixumLauncher?style=for-the-badge&label=%D0%B2%D0%B5%D1%80%D1%81%D0%B8%D1%8F&color=ff5fa2&labelColor=1a171c"></a>
  <a href="https://github.com/saxxumm/PrixumLauncher/releases"><img alt="Загрузки" src="https://img.shields.io/github/downloads/saxxumm/PrixumLauncher/total?style=for-the-badge&label=%D0%B7%D0%B0%D0%B3%D1%80%D1%83%D0%B7%D0%BA%D0%B8&color=ff5fa2&labelColor=1a171c"></a>
  <img alt="Windows, macOS, Linux" src="https://img.shields.io/badge/Windows%20%C2%B7%20macOS%20%C2%B7%20Linux-ff5fa2?style=for-the-badge&labelColor=1a171c">
  <a href="LICENSE"><img alt="GPL-3.0" src="https://img.shields.io/badge/GPL--3.0-ff5fa2?style=for-the-badge&label=%D0%BB%D0%B8%D1%86%D0%B5%D0%BD%D0%B7%D0%B8%D1%8F&labelColor=1a171c"></a>
</p>

<p align="center">
  <a href="https://github.com/saxxumm/PrixumLauncher/releases/latest"><b>⬇ Скачать</b></a>
  &nbsp;·&nbsp;
  <a href="#возможности">Возможности</a>
  &nbsp;·&nbsp;
  <a href="#свои-темы">Свои темы</a>
  &nbsp;·&nbsp;
  <a href="#сборка-из-исходников">Сборка</a>
</p>

<p align="center">
  <img alt="Главное окно Prixum Launcher" src="docs/screenshots/main.png" width="100%">
</p>

> Prixum Launcher — форк [Prism Launcher](https://github.com/PrismLauncher/PrismLauncher), независимый проект.

## Возможности

### Дизайн Nova

Боковая панель, карточки сборок с группами, поиск и сортировка, панель справа со временем игры и
большой кнопкой запуска. В комплекте семь тем: тёмная розовая **Prixum** по умолчанию и ещё шесть.

<p align="center">
  <img alt="Встроенные темы" src="docs/screenshots/themes.png" width="100%">
</p>

### Новая сборка — сразу с модами

Версия Minecraft, загрузчик и моды выбираются в одном окне. Кнопки «Моды», «Ресурспаки» и «Шейдеры» открывают
каталоги Modrinth и CurseForge, где показано только то, что подходит к выбранной версии и загрузчику, с зависимостями.
Всё отмеченное скачивается сразу после создания сборки.

<p align="center">
  <img alt="Новая сборка с модами" src="docs/screenshots/new-instance.png" width="100%">
</p>

### Моды, ресурспаки и шейдеры в редакторе сборки

Сверху — «Скачать», «Проверить обновления» и «Добавить файл», ниже — фильтры «Все», «Включённые» и «Отключённые».
Под названием мода видны его авторы, отключённые моды серые. Если выделить несколько модов, рядом появятся «Включить»,
«Отключить» и «Удалить». Если в сборке нет Iris или Oculus, на странице шейдеров появится подсказка с кнопкой,
которая скачает нужный мод.

<p align="center">
  <img alt="Моды в редакторе сборки" src="docs/screenshots/instance-mods.png" width="100%">
</p>

### Скачивание модов

Результаты поиска — карточки с иконкой, автором, числом скачиваний и кнопкой «Добавить». Справа описание и выбор
версии, эту панель можно свернуть кнопкой «Подробности». Modrinth и CurseForge переключаются кнопками сверху. Внизу
видно, что уже выбрано, и кнопка «Скачать (N)». В окне скачивания список файлов тоже сворачивается.

<p align="center">
  <img alt="Скачивание модов" src="docs/screenshots/mod-download.png" width="100%">
</p>

### Редактор тем

Меняй цвета, скругления, отступы, размеры плиток и панелей, шрифт или допиши свой QSS — весь лаунчер
перерисовывается сразу, а в окне предпросмотра видно, как выглядят кнопки, списки и вкладки.
Готовую тему можно сохранить как новую или выгрузить в папку, чтобы поделиться.

<p align="center">
  <img alt="Редактор тем" src="docs/screenshots/theme-editor.png" width="100%">
</p>

### Масштабирование рендера

Игра рендерится в пониженном разрешении (от 10 до 100 %) и растягивается до размера окна. Это поднимает FPS
на слабом железе или даёт пиксельный ретро-вид. На выбор четыре фильтра: **чёткие пиксели** без размытия,
**сглаживание**, **AMD FSR** и **NVIDIA NIS** с повышением резкости. Настраивается для всех сборок сразу
или для каждого отдельно, живой предпросмотр показывает результат ещё до запуска.

<p align="center">
  <img alt="Чёткие пиксели и сглаживание" src="docs/screenshots/filters.png" width="85%">
</p>

<p align="center">
  <img alt="Настройки масштабирования рендера" src="docs/screenshots/render-scaling.png" width="80%">
</p>

> [!NOTE]
> Масштабирование работает на Linux через [gamescope](https://github.com/ValveSoftware/gamescope):
> `sudo pacman -S gamescope`, `sudo apt install gamescope` или `sudo dnf install gamescope`.

### Скины как в Modrinth App

Слева большой игрок, его можно крутить мышкой, рядом выбор модели и плаща. Справа все сохранённые скины,
и каждый идёт на месте. Скин добавляется файлом, перетаскиванием, ником игрока или ссылкой: NameMC, прямой PNG,
textures.minecraft.net. Если в буфере обмена лежит ссылка на скин с NameMC, лаунчер сам предложит её добавить.
Для аккаунта Microsoft лаунчер сам подтягивает историю его скинов с [crafty.gg](https://crafty.gg) и добавляет
новые в сохранённые; это отключается переключателем «История скинов».

<p align="center">
  <img alt="Скины" src="docs/screenshots/skins.png" width="100%">
</p>

### Обои и матовое стекло

За списком сборок можно поставить свою картинку, тогда плитки сборок становятся матовым стеклом. Размытие и
затемнение настраиваются, превью видно прямо в настройках. По умолчанию выключено: «Настройки → Внешний вид → Обои».

<p align="center">
  <img alt="Обои и матовое стекло" src="docs/screenshots/wallpaper.png" width="100%">
</p>

### Новые настройки

Страницы собраны в группы, сверху поиск, который ищет по содержимому страниц и подсвечивает найденное. Под названием
страницы есть короткое описание, флажки стали переключателями, а темы выбираются по карточкам с цветами.
У котика теперь можно поменять размер.

<p align="center">
  <img alt="Новые настройки" src="docs/screenshots/settings.png" width="75%">
</p>

### Новые иконки

Свой набор иконок в настройках, редакторе сборки и меню. Иконки перекрашиваются под цвета активной темы.

<table>
  <tr>
    <td width="50%"><img alt="Настройки" src="docs/screenshots/settings.png"></td>
    <td width="50%"><img alt="Редактор сборки" src="docs/screenshots/instance-editor.png"></td>
  </tr>
</table>

### И всё остальное

- живой русский перевод: «сборка», «аккаунт», «ресурспаки», «скачать», без канцелярита;
- вход через аккаунт **Microsoft**, несколько аккаунтов;
- аккаунты **Ely.by** для серверов с авторизацией Ely.by и скинов Ely.by (нужен аккаунт Microsoft с купленным Minecraft);
- модпаки и моды с **Modrinth** и **CurseForge**, загрузчики **Fabric**, **Forge**, **NeoForge** и **Quilt**;
- автоматическая загрузка нужной **Java**;
- импорт и экспорт сборок, ярлыки на рабочем столе, portable-режим;
- при первом запуске Prixum предлагает перенести сборки, аккаунты и настройки из Prism Launcher —
  исходная папка при этом не меняется.

## Скачать

Все файлы — на странице [последнего релиза](https://github.com/saxxumm/PrixumLauncher/releases/latest),
их имена начинаются с `PrixumLauncher-`.

| Система | Установщик | Portable |
| --- | --- | --- |
| **Windows** 10 / 11 | `Windows-MSVC-Setup` · `.exe` | `Windows-MSVC-Portable` · `.zip` |
| **Windows** на ARM | `Windows-MSVC-arm64-Setup` · `.exe` | `Windows-MSVC-arm64-Portable` · `.zip` |
| **macOS** 13+ | `macOS` · `.dmg` | `macOS-Portable` · `.zip` |
| **Linux** | [`Linux-x86_64.AppImage`](https://github.com/saxxumm/PrixumLauncher/releases/latest/download/PrixumLauncher-Linux-x86_64.AppImage) | `Linux-Qt6-Portable` · `.tar.gz` |
| **Arch / CachyOS** | `prixumlauncher-*.pkg.tar.zst` | — |

Portable-версия хранит сборки, аккаунты и настройки внутри своей папки — её можно носить на флешке.

<details>
<summary><b>Windows или macOS не дают запустить?</b></summary>
<br>

Сборки не подписаны сертификатом разработчика.

- **Windows:** в окне SmartScreen нажми «Подробнее» → «Выполнить в любом случае».
- **macOS:** правый клик по приложению → «Открыть», либо в терминале:
  `xattr -dr com.apple.quarantine "/Applications/Prixum Launcher.app"`

</details>

## Свои темы

Тема — это папка в `themes/` с файлом `theme.json` и, если нужно, `style.qss`. Проще всего начать в редакторе тем
(**Темы → Редактор тем…**), но файлы можно править и вручную — лаунчер подхватывает изменения на лету.

```json
{
    "format": "nova",
    "name": "Моя тема",
    "base": "dark",
    "colors": { "accent": "#ff5fa2", "window": "#121014", "surface": "#1a171c" },
    "metrics": { "radius": 16, "cardWidth": 112, "iconSize": 48 }
}
```

Все цвета, размеры и правила для QSS описаны в [PRIXUM.md](PRIXUM.md).

## Сборка из исходников

Нужны Qt 6, CMake, Ninja, extra-cmake-modules и JDK 17 или новее.

```bash
git clone --recursive https://github.com/saxxumm/PrixumLauncher.git
cd PrixumLauncher
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLauncher_ENABLE_JAVA_DOWNLOADER=ON
cmake --build build
```

На Arch и CachyOS пакет собирается и ставится одной командой: `cd packaging/arch && makepkg -si`.
Установщики для Windows, macOS и Linux собирает GitHub Actions при публикации тега — подробности в [PRIXUM.md](PRIXUM.md).

## Лицензия

Код распространяется под [GPL-3.0](LICENSE), сторонние компоненты перечислены в [COPYING.md](COPYING.md).
Логотип основан на логотипе Prism Launcher и доступен под [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/).
