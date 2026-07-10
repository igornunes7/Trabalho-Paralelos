# Árvore Geradora Mínima com MPI

Este repositório contém a implementação sequencial e paralela de um algoritmo para encontrar a Árvore Geradora Mínima (AGM) de um grafo de grande escala.

O trabalho foi desenvolvido para a disciplina de Programação Paralela, utilizando linguagem C e MPI. A solução utiliza o algoritmo de Boruvka em conjunto com a estrutura Union-Find para controlar os componentes do grafo e evitar ciclos durante a construção da AGM.

## Objetivo

Comparar o desempenho entre uma versão sequencial e uma versão paralela do algoritmo, analisando tempo de execução, speedup, eficiência e comportamento da aplicação com diferentes quantidades de processos.

## Tecnologias utilizadas

- Linguagem C
- MPI
- Makefile
- Linux

## Execuções realizadas

Foram realizados testes com:

- Versão sequencial
- Versão paralela com 4 processos
- Versão paralela com 8 processos
- Versão paralela com 12 processos
- Versão paralela com 16 processos

Os resultados completos, gráficos, análise de desempenho e logs de execução estão disponíveis na documentação do projeto.

## Compilação e execução

Para compilar:
make

Para executar a versão sequencial:
make runseq

Para executar a versão paralela:
make run4, 
make run8, 
make run12, 
make run16

## Documentação

A documentação completa do trabalho está incluída no repositório em formato PDF, contendo a descrição do problema, decisões de implementação, comunicação entre máquinas, testes, gráficos e análise dos resultados.

## Autor

Igor Monteiro Nunes
