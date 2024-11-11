: ${CC:="clang"}
: ${CXX:="clang++"}
: ${CFLAGS:="-flto -g -ggdb"}
: ${LDFLAGS:=""}

http_serv="c-http-server/src/*.c c-http-server/C-Thread-Pool/thpool.c"
app="main.c utils.c memmap.c templating_lang.c"
link="-lsparkey -lpthread -lsnappy -lzstd"

mkdir -p obj
for file in $app $http_serv; do
    repl=${file//\//_}
    ${CC} -DHAS_ZSTD -c $file -o "obj/$repl.o" $CFLAGS
done
${CXX} obj/*.o -o out $link $LDFLAGS
