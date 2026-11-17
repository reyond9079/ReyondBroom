# RB-21. Локальный кандидат и результаты приёмки

Сборки `0.6.0-alpha.24-rb21.qa.3` изготовлены из `193a064ef2ce6728bde3dd39bd06957d564b0684`. В `93c790ce24b750940bd7e0dbe41e3df5b91cdbbb` доработан тест нативного UI. Код модуля тот же. Релиз, теги и слияние с main отсутствуют. RB-21 открыт.

## Геометрия и внешний вид

- Убраны все 865 оставшихся геометрических контактов. Маски брюк и ботинок Charcoal Utility теперь перекрываются. У Summer, Traveler и Winter кожа под внутренним краем проймы плавно уходит внутрь, сохраняется связная геометрия. [Стандартная матрица из 520 комбинаций](<qa>/acceptance-next/final-matrix.csv), [CSV контактов](<qa>/acceptance-next/final-contacts.csv)
- Дополнительные 160 комбинаций этих четырёх комплектов с изменённой высотой, плечом, предплечьем и голенью тоже прошли без пересечений. [Дополнительная матрица](<qa>/acceptance-next/extended-matrix.csv)
- Осмотрены нативные кадры до и после, включая профиль, вид сзади, противоположную сторону и ракурс в три четверти. [Ботинки](<qa>/acceptance-next/editor-boot-review.png), [проймы](<qa>/acceptance-next/editor-sleeves-review.png)

## Редактор, экспорты и игры

- Linux и Linux SDL2 прошли проверки открытия и сохранения старого Field Technician. Нативный Linux прогон подтвердил FTE-размещение, Quake III-компиляцию и перенос зависимостей. Отдельно проверены 41 сценарий Model Editor и Plant Editor из теста размещения. [Общий прогон](<qa>/acceptance-next/linux-combined/test-summary.md), [SDL2](<qa>/acceptance-next/linux-sdl2-document/results.json), [41 проверка](<qa>/acceptance-next/linux-viewport-x11/results.json)
- Каталог проверен повторно на локальном пакете. Все проверки прошли, включая 13 комплектов, 10 пресетов, перемотку и восстановление материалов и позы после перезапуска рендера. Тест выбирает строки внутри видимой части списка и сравнивает кадр после загрузки материалов с прежней строгой погрешностью. Дополнительно все 27 клипов выбраны и перемотаны через нативный UI с проверкой пикселей. [Каталог](<qa>/acceptance-next/linux-catalog-verified.log), [27 клипов](<qa>/acceptance-next/linux-all-clips-final.log)
- Свежие IQM затронутых комплектов проиграли общую ходьбу в отдельном FTE. Zircon воспроизвёл MDL в покое, ходьбе и падении, Half-Life загрузил свежий Studio MDL из теста экспортера и проиграл ходьбу, бег и падение. Yamagi загрузил новый MD2, OpenArena показала движение MD3 в трёх диапазонах на скомпилированной карте. [Реальные кадры игр](<qa>/acceptance-next/engines-review.png), [FTE](<qa>/acceptance-next/fte-game-boots/report.json), [Zircon](<qa>/acceptance-next/quake1-runtime-check.json), [Half-Life](<qa>/acceptance-next/windows-goldsrc/report.json), [Yamagi](<qa>/acceptance-next/quake2-native-engine.log), [OpenArena](<qa>/acceptance-next/openarena/runtime-check.json)
- Прошли ASan/UBSan, проверки активов, независимые декодеры и тест запрета Character Creator в Mapping Jams. Khronos Validator вернул 0 ошибок и 0 предупреждений. [ASan/UBSan](<qa>/acceptance-next/linux-combined/CORE-ASAN-UBSAN.log), [GLB](<qa>/acceptance-next/khronos-gltf.json), [Mapping Jams](<qa>/acceptance-next/character-disabled-in-jam.log)
- У трёх локальных пакетов проверены версия, манифест, архивные SHA-256, соответствующие исходники и запуск. Отрицательные сценарии защиты релиза прошли. [Windows](<qa>/acceptance-next/package-smoke-Windows-x64/results.json), [Linux](<qa>/acceptance-next/package-smoke-Linux-x64/results.json), [Linux SDL2](<qa>/acceptance-next/package-smoke-Linux-SDL2-x64/results.json), [защита релиза](<qa>/acceptance-next/linux-combined/RELEASE-LOCK-DRY-RUN.log)

## Что ещё открыто в Jira

- Полный Windows GUI-прогон прерван после потери тестовым окном фокуса. Проверку запуска пакета и тест Half-Life выполнил на отдельном рабочем столе. Полная матрица докинга, масштабов и остального Windows UI для кандидата ещё не подтверждена.
- В повторном стресс-тесте Windows сборка и скиннинг 1000 персонажей прошли, p95 сборки полного меша 65,5 мс. В Linux p95 40,9 мс. Требуемый отдельный замер CPU и GPU для кадра и тесты памяти десяти одновременных персонажей остаются открытыми. [Windows CPU](<qa>/acceptance-next/windows-stress-recheck.log), [Linux CPU](<qa>/acceptance-next/linux-combined/CORE-ASSETS-EXPORT-DECODERS.log)
- Проверка размещения Model Editor и Plant Editor покрывает 41 сценарий. Полные регрессионные наборы и остальные обязательные пункты Jira пока не сведены в полностью зелёную матрицу. [RB-21](https://reyond.atlassian.net/browse/RB-21)
