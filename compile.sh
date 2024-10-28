http_serv=""
http_serv+="c-http-server/src/*.c c-http-server/C-Thread-Pool/thpool.c"
http_serv+=" -DHAS_ZSTD -lzstd"

# TODO: broken for absolutely no reason
#http_serv+=" -DHAS_ZLIB -lz"

app=""
app+="main.c utils.c"

: ${CC:="clang"}

$CC $app $http_serv -o out -O0 -g -glldb
