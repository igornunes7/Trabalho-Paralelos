#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <float.h>

#define VERTICES 10000000
#define ARESTAS 800000000LL
#define BLOCO 1000000

typedef struct {
    int origem;
    int destino;
    double peso;
} Aresta;

/* Busca o representante do conjunto usando compressão de caminho.
   Essa função faz parte da estrutura Union-Find. */
int find(int raiz[], int x) {
    if (raiz[x] != x)
        raiz[x] = find(raiz, raiz[x]);

    return raiz[x];
}

/* Une dois componentes da árvore usando rank.
   Isso evita que a estrutura fique muito desbalanceada. */
void unionSet(int raiz[], int rank[], int a, int b) {
    int raizA = find(raiz, a);
    int raizB = find(raiz, b);

    if (raizA == raizB)
        return;

    if (rank[raizA] < rank[raizB]) {
        raiz[raizA] = raizB;
    } else if (rank[raizA] > rank[raizB]) {
        raiz[raizB] = raizA;
    } else {
        raiz[raizB] = raizA;
        rank[raizA]++;
    }
}

/* Verifica se a aresta 'a' é melhor que a aresta 'b'.
   A comparação usa o peso e, em caso de empate, origem e destino. */
int melhor(Aresta a, Aresta b) {
    if (b.origem == -1)
        return 1;

    if (a.peso < b.peso)
        return 1;

    if (a.peso == b.peso) {
        if (a.origem < b.origem)
            return 1;

        if (a.origem == b.origem && a.destino < b.destino)
            return 1;
    }

    return 0;
}

/* Função usada pelo qsort para ordenar as arestas da AGM.
   A ordenação é feita por peso e depois por origem/destino para desempate. */
int compararArestasPorPeso(const void *a, const void *b) {
    const Aresta *arestaA = (const Aresta *)a;
    const Aresta *arestaB = (const Aresta *)b;

    if (arestaA->peso < arestaB->peso)
        return -1;

    if (arestaA->peso > arestaB->peso)
        return 1;

    if (arestaA->origem < arestaB->origem)
        return -1;

    if (arestaA->origem > arestaB->origem)
        return 1;

    if (arestaA->destino < arestaB->destino)
        return -1;

    if (arestaA->destino > arestaB->destino)
        return 1;

    return 0;
}

/* Calcula o tempo decorrido entre dois pontos medidos com clock_gettime. */
double calcularTempo(struct timespec inicio, struct timespec fim) {
    return (double)(fim.tv_sec - inicio.tv_sec) +
           (double)(fim.tv_nsec - inicio.tv_nsec) / 1000000000.0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: ./seq <arquivo_entrada>\n");
        return 1;
    }

    int V = VERTICES;
    long long E = ARESTAS;

    /* Vetores principais:
       raiz/rank: Union-Find
       menor: menor aresta encontrada para cada componente
       agm: arestas escolhidas para a árvore geradora mínima
       buffer: leitura do arquivo em blocos */
    int *raiz = malloc((size_t)V * sizeof(int));
    int *rank = calloc((size_t)V, sizeof(int));
    Aresta *menor = malloc((size_t)V * sizeof(Aresta));
    Aresta *agm = malloc((size_t)(V - 1) * sizeof(Aresta));
    Aresta *buffer = malloc((size_t)BLOCO * sizeof(Aresta));

    if (!raiz || !rank || !menor || !agm || !buffer) {
        printf("Erro de memoria.\n");

        free(raiz);
        free(rank);
        free(menor);
        free(agm);
        free(buffer);

        return 1;
    }

    /* Cada vértice começa como um componente separado. */
    for (int i = 0; i < V; i++)
        raiz[i] = i;

    int componentes = V;
    int qtdAGM = 0;

    struct timespec inicio;
    struct timespec fimAlgoritmo;

    clock_gettime(CLOCK_MONOTONIC, &inicio);

    /* Laço principal do algoritmo de Borůvka.
       A cada rodada, encontra a menor aresta de saída de cada componente. */
    while (componentes > 1) {
        /* Inicializa o vetor de menores arestas.
           origem = -1 indica que ainda não foi encontrada uma aresta válida. */
        for (int i = 0; i < V; i++) {
            menor[i].origem = -1;
            menor[i].destino = -1;
            menor[i].peso = 0.0;
        }

        FILE *entrada = fopen(argv[1], "rb");

        if (entrada == NULL) {
            printf("Erro ao abrir arquivo de entrada.\n");

            free(raiz);
            free(rank);
            free(menor);
            free(agm);
            free(buffer);

            return 1;
        }

        long long lidasTotal = 0;

        /* O arquivo é lido em blocos para evitar carregar todas as arestas de uma vez. */
        while (lidasTotal < E) {
            long long restante = E - lidasTotal;
            size_t qtdLer;

            if (restante < BLOCO)
                qtdLer = (size_t)restante;
            else
                qtdLer = (size_t)BLOCO;

            size_t lidas = fread(buffer, sizeof(Aresta), qtdLer, entrada);

            if (lidas == 0)
                break;

            /* Percorre as arestas lidas no bloco atual. */
            for (size_t j = 0; j < lidas; j++) {
                Aresta atual = buffer[j];

                int u = atual.origem;
                int v = atual.destino;
                double peso = atual.peso;

                /* Valida a aresta antes de processar. */
                if (u < 0 || u >= V || v < 0 || v >= V)
                    continue;

                if (u == v)
                    continue;

                if (peso < 0.0 || peso == DBL_MAX)
                    continue;

                int compU = find(raiz, u);
                int compV = find(raiz, v);

                if (compU != compV) {
                    if (melhor(atual, menor[compU]))
                        menor[compU] = atual;

                    if (melhor(atual, menor[compV]))
                        menor[compV] = atual;
                }
            }

            lidasTotal += (long long)lidas;
        }

        fclose(entrada);

        if (lidasTotal != E) {
            printf("Foram lidas %lld arestas, mas eram esperadas %lld.\n",
                   lidasTotal, E);

            free(raiz);
            free(rank);
            free(menor);
            free(agm);
            free(buffer);

            return 1;
        }

        int adicionou = 0;

        /* Após encontrar as menores arestas, tenta unir os componentes.*/
        for (int i = 0; i < V; i++) {
            if (menor[i].origem != -1) {
                int u = menor[i].origem;
                int v = menor[i].destino;

                int compU = find(raiz, u);
                int compV = find(raiz, v);

                if (compU != compV) {
                    agm[qtdAGM++] = menor[i];

                    unionSet(raiz, rank, compU, compV);

                    componentes--;
                    adicionou = 1;
                }
            }
        }

        /* Se nenhuma união foi feita, o algoritmo para para evitar loop infinito. */
        if (!adicionou)
            break;
    }

    /* Ordenação, soma e escrita do arquivo não entram no tempo total. */
    clock_gettime(CLOCK_MONOTONIC, &fimAlgoritmo);

    /* Ordena a AGM apenas para padronizar a soma e a saída. */
    qsort(agm, (size_t)qtdAGM, sizeof(Aresta), compararArestasPorPeso);

    double pesoTotal = 0.0;

    for (int i = 0; i < qtdAGM; i++)
        pesoTotal += agm[i].peso;

    double tempoTotal = calcularTempo(inicio, fimAlgoritmo);

    FILE *saida = fopen("saida_seq.txt", "w");

    if (saida == NULL) {
        printf("Erro ao criar arquivo de saida.\n");

        free(raiz);
        free(rank);
        free(menor);
        free(agm);
        free(buffer);

        return 1;
    }

    fprintf(saida, "Tempo total: %.6f segundos\n\n", tempoTotal);
    fprintf(saida, "Arestas da AGM:\n");

    for (int i = 0; i < qtdAGM; i++)
        fprintf(saida, "%d %.12f %d\n", agm[i].origem, agm[i].peso, agm[i].destino);

    fclose(saida);

    printf("\n");
    printf("Peso total: %.12f\n", pesoTotal);
    printf("Tempo total: %.6f segundos\n", tempoTotal);
    printf("Arquivo gerado: saida_seq.txt\n");

    free(raiz);
    free(rank);
    free(menor);
    free(agm);
    free(buffer);

    return 0;
}
