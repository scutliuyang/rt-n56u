* ИНСТРУКЦИЯ ПО СБОРКЕ ПРОШИВКИ *

1) Для сборки прошивки требуется Linux окружение 32 или 64 бита. Сборка прошивки
   протестирована и рекомендуется на Linux дистрибутивах Debian 'wheezy' 7.8.0 и
   Debian 'jessie' 8.0.0.
2) Первым делом необходимо собрать кросс-toolchain (набор для кросс-компиляции)
   под MIPS32_R2 CPU, состоящий из пакетов binutils-2.24, gcc-447, uclibc-0.9.33.2.
   Перейти в директорию toolchain-mipsel и выполнить скрипт сборки. Сборка
   кросс-toolchain занимает от 10 до минут до нескольких часов, в зависимости 
   от типа CPU хоста. Если кросс-toolchain уже собран, этот пункт пропускается.
3) Отредактировать вручную файл ".config" в корне дерева. Для комментирования 
   строк используется символ #. Если требуется отключить параметр, не следует 
   менять y на n, необходимо закомментировать строку целиком. Изменить параметр 
   "CONFIG_TOOLCHAIN_DIR=" на актуальный путь до директории с кросс-toolchain.
4) Собрать прошивку, запустив скрипт "build_firmware". Сборка прошивки может 
   занимать от 20 минут до нескольких часов. После сборки файл образа прошивки 
   (*.trx) будет находиться в директории images.


* ПРИМЕЧАНИЕ *

Для сборки прошивки из под Linux дистрибутива Debian 'wheezy'/'jessie' требуются
пакеты (на других дистрибутивах состав и наименования пакетов могут отличаться):
- sudo
- build-essential
- gawk
- pkg-config
- gettext
- autoconf
- automake
- libtool
- bison
- flex
- zlib1g-dev

Для сборки кросс-toolchain требуются дополнительные пакеты:
- texinfo
- libgmp3-dev
- libmpfr-dev
- libmpc-dev



-
10.05.2015
Padavan
