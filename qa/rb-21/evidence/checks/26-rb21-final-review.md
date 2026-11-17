# RB-21. Локальная приёмка Character Creator

Локальный коммит `3d0aef5402af562aae37d7c6667b61b544656275`. [Сводка проверок и SHA-256 модуля](rb21-final-results.json)

## Windows UI

- В нативном редакторе проверены создание, открытие, сохранение, Save As, отмена, импорт и переносимость GLB/RBW, сохранение старого Field Technician, каталог, размещение в FTE и сборка карты Quake III через обычный интерфейс. [Документы](windows-document/results.json) · [Каталог](windows-catalog/results.json) · [FTE](windows-fte-placement/results.json) · [Quake III](windows-q3-placement/results.json)
- Все 27 клипов выбраны и перемотаны с проверкой изменений пикселей нативного вьюпорта. [Клипы Windows](windows-timeline-all/results.json)
- При 1280×720 и фактических 200% тест воспроизвёл пропадание вьюпорта и ползунка. В тесном окне настройки предпросмотра доступны из View options, а перемотка расположена под Play. Исправленная Windows-сборка отрисовала модель, открыла Face по клику и проиграла анимацию. [Снимок](windows-layout-fixed/1280x720-dark-200/face-playing.png)
- Все 24 комбинации прошли. Матрица размера окна, масштаба и темы сверяет фактический размер кнопки, долю светлых пикселей, отрисовку модели, клики Face, перемотку и Play. [Первый проход](windows-layout-final-matrix/results.json) · [Повтор](windows-layout-replay/results.json)

- Первый запуск матрицы использовал параметры по умолчанию. В итоговый отчёт вошли 24 подтверждённых сценария с измеренной высотой элементов и проверенной палитрой. Три дополнительных сценария проверили переключение Dockable panes и восстановление плавающей компоновки. [Матрица](rb21-final-results.json) · [Докинг](windows-layout-docking/results.json)

## CPU и GPU

- RTX 4060 Laptop GPU, драйвер 591.74, 1920×1080, VSync выключен, без лимита FPS. Анимация run_forward, 13 618 треугольников в Balanced. OpenGL замерен асинхронным GL_TIME_ELAPSED, поток рендера CPU замерен через GetThreadTimes. На исправленной сборке поток рендера CPU в среднем использует 3,65 мс на кадр, GPU 1,55 мс при p95 2,06 мс. Без инструментирования среднее значение 210,4 FPS, p95 интервала 7,33 мс. С таймерами среднее значение 173,9 FPS. [Повторный замер](windows-final-cpu-gpu.json) · [Исходный замер](windows-cpu-gpu-results.json)
- В редакторе без открытого Character Creator сверены обычная карта, Model Editor с 5 000 треугольников и Plant Editor с English oak. Использован один инструментированный движок с модулями до и после RB-21, по 480 кадров на сценарий в двух противоположных порядках запуска. Время CPU на отправку кадра изменилось на +3,3%, +5,9% и −3,1% соответственно. Максимальное замедление по времени работы CPU или GPU составило 6,5% в Model Editor. [Раздельные метрики](windows-regression-timings.json) · [Сравнение сцен](windows-existing-tool-performance/comparison.png)

## Регрессии

- Model Editor прошёл нативные прогоны в Windows и Linux. Проверены иерархия, UV, перемещение, анимация, камера, рендер материалов, статические и анимированные экспорты. Старый нативный тест открывает свой инспектор, использует синтетический Quake MDL и прекращает работу при пустой модели. [Windows](windows-reg-legacy-model.log) · [Старый Linux](reg-model-legacy-rerun.log) · [Моделирование Linux](reg-model-bb.log)
- Plant Editor прошёл генерацию, проверку библиотек, нативный интерфейс, создание и сохранение сущности карты, Plant Brush, независимое повторное чтение GLB/OBJ и реальную компиляцию карты. [Интерфейс](reg-plant_ui.log) · [Экспорт](reg-plant-exports.log) · [Сборка](reg-plant-map.log) · [Plant Brush](reg-plant-brush.log)
- MAP прошёл редактирование, клиппинг, UV, откат, повтор и повторное открытие. Подтверждены 16 брашей и нулевой дрейф плоскостей на отдельном Windows round-trip. Рабочая область восстанавливает компоновку и масштаб после перезапуска. Четыре Linux-сценария размещения прошли по 41 проверке. [Windows редактирование](windows-reg-editing-post-fix.log) · [MAP round-trip](windows-reg-map-roundtrip-post-fix.log) · [Рабочая область](windows-reg-workspace-post-fix.log) · [Профили](core/profile-unprivileged.log)
- Тест раскладки и клавиатуры дожидается завершения поиска действия. Dark и Light на двух размерах прошли повторно. [Dark](reg-appearance-filtered-dark-1920.log) · [Light](reg-appearance-filtered-light-1920.log)
- Исправленная Linux-сборка повторно прошла нативный каталог, смену табов, перемотку, перезапуск рендера, открытие и запись старого файла. [Каталог](reg-linux-final-catalog.log) · [Документ](reg-linux-final-document.log)
- Карты, одежда и анимационные ассеты совпадают с проверенным кандидатом. Предыдущие 520 стандартных и 160 расширенных проверок геометрии, автономные FTE, Half-Life, Zircon, Yamagi и OpenArena сохраняют применимость. [Предыдущий инженерный прогон](../acceptance-next/rb21-acceptance-report.md)

## Нагрузка и границы проверки

- Windows и Linux сгенерировали по 1 000 персонажей и записали с повторным открытием по 100 настоящих файлов .rbchar. p95 полной сборки исходных мешей составил 55,04 мс в Windows и 35,65 мс в Linux. Полные меши этой выборки превышали 20 тысяч треугольников. [Windows](core/character-roundtrips-windows.log) · [Linux](core/character-roundtrips-linux.log)
- Шесть раз переключена пустая карта и сцена с десятью видимыми экземплярами одного IQM. После прогрева частная память для заполненной сцены удержалась в диапазоне 788–790 МиБ. Счётчик FTE показал до 114 draw calls на сцене с моделями и 31 на пустой. Это короткий прогон с общим набором текстур. [Снимок сцены](windows-visible-characters-profiled/memory-6-ten.png) · [Память процесса](windows-visible-characters-profiled/memory.json) · [Счётчики FTE](windows-visible-characters-profiled/draw-calls.png)
- Три оставшиеся проверки завершены. [Результаты завершения RB-21](rb21-completion.md)

Публикация и теги не создавались.
