#/bin/bash
echo "#ifndef SSV6200_UART_BIN_H" >$1
echo "#define SSV6200_UART_BIN_H" >>$1
echo "static const \c" >>$1
xxd -i ssv6200-uart.bin >>$1
sed -i '/const unsigned char ssv6200_uart_bin/i #ifdef APP_INCLUDE_WIFI_FW' $1
sed -i '/ssv6200_uart_bin_len/i #endif' $1
echo "#endif" >>$1
