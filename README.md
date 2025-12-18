# PingPongOS

Este projeto é uma implementação de um sistema operacional simples, chamado PingPongOS. Ele foi desenvolvido como parte de um trabalho acadêmico para a disciplina de Sistemas Operacionais.

O PingPongOS implementa funcionalidades básicas de um sistema operacional, como:
- Gerência de tarefas
- Escalonamento de tarefas com prioridades
- Semáforos
- Filas de mensagens

## Estrutura do Repositório

O repositório está organizado da seguinte forma:
- `app/`: Contém o código da aplicação de exemplo (`pingpong.c`).
- `include/`: Contém os arquivos de cabeçalho do sistema operacional.
- `src/`: Contém o código fonte do sistema operacional.
- `Makefile`: Arquivo para compilação do projeto.
- `README.md`: Este arquivo.

## Como Compilar e Executar

Para compilar o projeto, utilize o comando `make`:
```bash
make
```
Este comando irá compilar o código fonte e gerar um executável chamado `pingpong`.

Para executar a aplicação, utilize o comando `make run` ou execute o arquivo `pingpong` diretamente:
```bash
make run
```
ou
```bash
./pingpong
```

## Limpeza

Para limpar os arquivos de objeto gerados durante a compilação, utilize o comando `make clean`:
```bash
make clean
```

Para limpar todos os arquivos gerados (executável e arquivos de objeto), utilize o comando `make purge`:
```bash
make purge
```
