# IBKS pintos labs 1-5 (3 semester) by Eldar Timraleev.
# Task planner by Arina Matvienko.

# Данный проект создан в целях реализации планировщика задач (демона) для ОС pintos как основа для дополнительной лабораторной работы для студентов СПбПУ.

# Команды:
Следующие команды ориентированы на работу с планировщиком задач для данной ОС.

Скомпилировать проект:
 cd src/userprog
 make

Создать диск:
 cd src/userprog
 pintos-mkdisk filesys.dsk --filesys-size=2

Форматировать диск:
 pintos --qemu --disk=filesys.dsk -- -f -q

Вывести содержимое root директории диска:
 pintos --qemu --disk=filesys.dsk -- -q ls

Или содержимое конкретного файла:
 pintos --qemu --disk=filesys.dsk -- -q cat 'file.txt'

Запустить проект:
 pintos --qemu --disk=filesys.dsk --

Скомпилировать пользовательскую программу (как пример работы с планировщиком):
 cd src/examples
 make

Записать пользовательскую программу на диск:
 cd src/userprog
 pintos --qemu --disk=filesys.dsk -p ../examples/planner_add -a planner_add -- -q

Добавить задачу в планировщик через пользовательскую программу:
Формат времени $TIME: ГГГГ:ММ:ДД:чч:мм:сс
 pintos --qemu --disk=filesys.dsk -- -q run 'planner_add $NAME $TIME'
