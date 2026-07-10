#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>
#include <mpi.h>
#include <unistd.h>

#define VERTICES 10000000
#define ARESTAS 800000000LL
#define BLOCO 1000000
#define LIMIAR_COMPACTACAO 30

#define TAG_QTD 10
#define TAG_DADOS 11

typedef struct {
    int origem;
    int destino;
    double peso;
} Aresta;

typedef struct {
    int u;
    int v;
} Uniao;

/* Busca o representante do conjunto usando Union-Find.
   Aqui também é feita compressão de caminho para acelerar buscas futuras. */
int find(int raiz[], int x) {
    while (raiz[x] != x) {
        raiz[x] = raiz[raiz[x]];
        x = raiz[x];
    }

    return x;
}

/* Une dois componentes usando altura.
   Retorna 1 se a união foi feita e 0 se os vértices já estavam no mesmo componente. */
int unionSet(int raiz[], unsigned char altura[], int a, int b) {
    int raizA = find(raiz, a);
    int raizB = find(raiz, b);

    if (raizA == raizB)
        return 0;

    if (altura[raizA] < altura[raizB]) {
        raiz[raizA] = raizB;
    } else if (altura[raizA] > altura[raizB]) {
        raiz[raizB] = raizA;
    } else {
        if (raizA < raizB) {
            raiz[raizB] = raizA;
            altura[raizA]++;
        } else {
            raiz[raizA] = raizB;
            altura[raizB]++;
        }
    }

    return 1;
}

/* Compara duas arestas e decide se 'a' é melhor que 'b'.
   Primeiro compara pelo peso. Em empate, usa origem e destino para desempatar. */
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
   A ordenação é usada para padronizar a soma final do peso. */
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

/* Operação personalizada usada no MPI_Reduce.
   Ela combina as menores arestas encontradas por cada processo. */
void reducao_menor_aresta(void *entrada, void *saida, int *len, MPI_Datatype *tipo) {
    (void)tipo;

    Aresta *vetorEntrada = (Aresta *)entrada;
    Aresta *vetorAtual = (Aresta *)saida;

    for (int i = 0; i < *len; i++) {
        if (vetorEntrada[i].origem != -1 && melhor(vetorEntrada[i], vetorAtual[i]))
            vetorAtual[i] = vetorEntrada[i];
    }
}

int main(int argc, char *argv[]) {
    int rank;
    int size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 2) {
        if (rank == 0)
            printf("Uso: mpirun -np <processos> ./par <entrada.bin>\n");

        MPI_Finalize();
        return 1;
    }

    int V = VERTICES;
    long long E = ARESTAS;

    /* Criação do tipo MPI para a struct Aresta.
       Isso permite enviar e receber Aresta diretamente com MPI_Send e MPI_Recv. */
    MPI_Datatype MPI_ARESTA;
    MPI_Datatype tiposAresta[3] = {MPI_INT, MPI_INT, MPI_DOUBLE};
    int blocosAresta[3] = {1, 1, 1};

    MPI_Aint deslocamentosAresta[3];

    deslocamentosAresta[0] = offsetof(Aresta, origem);
    deslocamentosAresta[1] = offsetof(Aresta, destino);
    deslocamentosAresta[2] = offsetof(Aresta, peso);

    MPI_Type_create_struct(3, blocosAresta, deslocamentosAresta, tiposAresta, &MPI_ARESTA);
    MPI_Type_commit(&MPI_ARESTA);

    /* Criação do tipo MPI para a struct Uniao.
       Esse tipo é usado para enviar as uniões que todos os ranks precisam repetir. */
    MPI_Datatype MPI_UNIAO;
    MPI_Datatype tiposUniao[2] = {MPI_INT, MPI_INT};
    int blocosUniao[2] = {1, 1};
    MPI_Aint deslocamentosUniao[2];

    deslocamentosUniao[0] = offsetof(Uniao, u);
    deslocamentosUniao[1] = offsetof(Uniao, v);

    MPI_Type_create_struct(2, blocosUniao, deslocamentosUniao, tiposUniao, &MPI_UNIAO);
    MPI_Type_commit(&MPI_UNIAO);

    /* Cria a operação de redução personalizada para escolher a menor aresta. */
    MPI_Op op_menor_aresta;

    MPI_Op_create(reducao_menor_aresta, 1, &op_menor_aresta);

    /* Cada rank calcula qual parte do arquivo ele deve receber. */
    long long inicioLocal = (long long)rank * E / size;
    long long fimLocal = (long long)(rank + 1) * E / size;
    long long nLocais = fimLocal - inicioLocal;

    Aresta *arestasLocais = malloc((size_t)nLocais * sizeof(Aresta));

    Aresta *buffer = NULL;

    if (rank == 0)
        buffer = malloc((size_t)BLOCO * sizeof(Aresta));

    if (!arestasLocais || (rank == 0 && !buffer)) {
        printf("Rank %d: erro de memoria nas arestas locais.\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    double inicioTempo = MPI_Wtime();

    /* Apenas o rank 0 abre o arquivo de entrada.
       Ele lê as arestas em blocos e distribui para os outros ranks. */
    if (rank == 0) {
        FILE *entrada = fopen(argv[1], "rb");

        if (entrada == NULL) {
            printf("Rank 0: erro ao abrir %s\n", argv[1]);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        for (int r = 0; r < size; r++) {
            long long ini = (long long)r * E / size;
            long long fim = (long long)(r + 1) * E / size;
            long long total = fim - ini;
            off_t deslocamento = (off_t)ini * (off_t)sizeof(Aresta);

            if (fseeko(entrada, deslocamento, SEEK_SET) != 0) {
                printf("Rank 0: erro ao acessar a parte do rank %d.\n", r);
                fclose(entrada);
                MPI_Abort(MPI_COMM_WORLD, 1);
            }

            long long enviadas = 0;

            while (enviadas < total) {
                long long restante = total - enviadas;
                int qtd;

                if (restante < BLOCO)
                    qtd = (int)restante;
                else
                    qtd = BLOCO;

                Aresta *destinoLeitura;

                if (r == 0)
                    destinoLeitura = &arestasLocais[enviadas];
                else
                    destinoLeitura = buffer;

                size_t lidas = fread(destinoLeitura, sizeof(Aresta), (size_t)qtd, entrada);

                if (lidas == 0) {
                    printf("Rank 0: erro ou fim inesperado do arquivo.\n");
                    printf("Arestas lidas para o rank %d: %lld de %lld\n", r, enviadas, total);
                    fclose(entrada);
                    MPI_Abort(MPI_COMM_WORLD, 1);
                }

                qtd = (int)lidas;

                if (r != 0) {
                    MPI_Send(&qtd, 1, MPI_INT, r, TAG_QTD, MPI_COMM_WORLD);
                    MPI_Send(buffer, qtd, MPI_ARESTA, r, TAG_DADOS, MPI_COMM_WORLD);
                }

                enviadas += (long long)lidas;
            }

            if (r != 0) {
                int fimEnvio = 0;
                MPI_Send(&fimEnvio, 1, MPI_INT, r, TAG_QTD, MPI_COMM_WORLD);
            }
        }

        fclose(entrada);
    } else {
        /* Ranks diferentes de 0 recebem suas arestas em blocos. */
        long long recebidas = 0;

        while (1) {
            int qtd;

            MPI_Recv(&qtd, 1, MPI_INT, 0, TAG_QTD, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            if (qtd == 0)
                break;

            if (recebidas + qtd > nLocais) {
                printf("Rank %d: recebeu mais arestas do que o esperado.\n", rank);
                MPI_Abort(MPI_COMM_WORLD, 1);
            }

            MPI_Recv(&arestasLocais[recebidas], qtd, MPI_ARESTA, 0, TAG_DADOS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            recebidas += qtd;
        }

        if (recebidas != nLocais) {
            printf("Rank %d: recebeu %lld arestas, mas esperava %lld.\n", rank, recebidas, nLocais);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }

    free(buffer);

    MPI_Barrier(MPI_COMM_WORLD);

    /* Estruturas principais do Borůvka paralelo. */
    int *raiz = malloc((size_t)V * sizeof(int));
    unsigned char *altura = calloc((size_t)V, sizeof(unsigned char));

    int *componentesAtivos = malloc((size_t)V * sizeof(int));
    int *mapaComponente = malloc((size_t)V * sizeof(int));
    int *novaLista = malloc((size_t)V * sizeof(int));
    unsigned char *marca = calloc((size_t)V, sizeof(unsigned char));

    Aresta *menorLocal = malloc((size_t)V * sizeof(Aresta));
    Aresta *menorGlobal = NULL;
    Aresta *agm = NULL;

    Uniao *unioes = malloc((size_t)(V - 1) * sizeof(Uniao));

    if (rank == 0) {
        menorGlobal = malloc((size_t)V * sizeof(Aresta));
        agm = malloc((size_t)(V - 1) * sizeof(Aresta));
    }

    if (!raiz || !altura || !componentesAtivos || !mapaComponente ||
        !novaLista || !marca || !menorLocal || !unioes ||
        (rank == 0 && (!menorGlobal || !agm))) {
        printf("Rank %d: erro de memoria.\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    /* Inicialmente cada vértice é um componente. */
    for (int i = 0; i < V; i++) {
        raiz[i] = i;
        componentesAtivos[i] = i;
        mapaComponente[i] = i;
    }

    int qtdComponentes = V;
    int qtdAGM = 0;
    int finalizado = 0;

    char hostname[256];

    gethostname(hostname, sizeof(hostname));

    printf("[rank %d/%d] rodando em %s, recebeu %lld arestas locais\n", rank, size, hostname, nLocais);
    fflush(stdout);

    MPI_Barrier(MPI_COMM_WORLD);

    /* Laço principal do Borůvka paralelo. */
    while (!finalizado) {
        /* Cada posição representa a menor aresta encontrada para um componente ativo. */
        for (int i = 0; i < qtdComponentes; i++)
            menorLocal[i].origem = -1;

        long long internas = 0;

        /* Cada rank percorre apenas suas arestas locais. */
        for (long long i = 0; i < nLocais; i++) {
            Aresta aresta = arestasLocais[i];

            int raizU = raiz[aresta.origem];
            int raizV = raiz[aresta.destino];

            if (raizU == raizV) {
                internas++;
                continue;
            }

            int posU = mapaComponente[raizU];
            int posV = mapaComponente[raizV];

            if (melhor(aresta, menorLocal[posU]))
                menorLocal[posU] = aresta;

            if (melhor(aresta, menorLocal[posV]))
                menorLocal[posV] = aresta;
        }

        /* Junta as menores arestas locais de todos os ranks no rank 0. */
        MPI_Reduce(menorLocal, rank == 0 ? menorGlobal : NULL, qtdComponentes, MPI_ARESTA, op_menor_aresta, 0, MPI_COMM_WORLD);

        int qtdUnioes = 0;

        /* O rank 0 escolhe as arestas que realmente entram na AGM. */
        if (rank == 0) {
            for (int i = 0; i < qtdComponentes; i++) {
                if (menorGlobal[i].origem == -1)
                    continue;

                int u = menorGlobal[i].origem;
                int v = menorGlobal[i].destino;

                int raizU = find(raiz, u);
                int raizV = find(raiz, v);

                if (raizU != raizV) {
                    agm[qtdAGM] = menorGlobal[i];
                    qtdAGM++;

                    unioes[qtdUnioes].u = raizU;
                    unioes[qtdUnioes].v = raizV;
                    qtdUnioes++;

                    unionSet(raiz, altura, raizU, raizV);
                }
            }
        }

        /* O rank 0 informa quantas uniões foram feitas. */
        MPI_Bcast(&qtdUnioes, 1, MPI_INT, 0, MPI_COMM_WORLD);

        /* Depois envia a lista de uniões para todos os ranks. */
        if (qtdUnioes > 0)
            MPI_Bcast(unioes, qtdUnioes, MPI_UNIAO, 0, MPI_COMM_WORLD);

        /* Os outros ranks repetem as mesmas uniões para manter o Union-Find igual. */
        if (rank != 0) {
            for (int i = 0; i < qtdUnioes; i++)
                unionSet(raiz, altura, unioes[i].u, unioes[i].v);
        }

        if (qtdUnioes == 0) {
            finalizado = 1;
            continue;
        }

        /* Atualiza todas as raízes depois das uniões. */
        for (int i = 0; i < V; i++)
            raiz[i] = find(raiz, i);

        int novaQtd = 0;

        /* Recria a lista de componentes ativos sem repetição. */
        for (int i = 0; i < qtdComponentes; i++) {
            int r = raiz[componentesAtivos[i]];

            if (!marca[r]) {
                marca[r] = 1;
                novaLista[novaQtd] = r;
                novaQtd++;
            }
        }

        int *temporario = componentesAtivos;
        componentesAtivos = novaLista;
        novaLista = temporario;

        for (int i = 0; i < novaQtd; i++) {
            mapaComponente[componentesAtivos[i]] = i;
            marca[componentesAtivos[i]] = 0;
        }

        qtdComponentes = novaQtd;

        if (qtdComponentes == 1) {
            finalizado = 1;
            continue;
        }

        /* Remove arestas internas quando muitas arestas locais já não são mais úteis. */
        if (nLocais > 0 && internas * 100 >= nLocais * LIMIAR_COMPACTACAO) {
            long long novaQuantidade = 0;

            for (long long i = 0; i < nLocais; i++) {
                int u = arestasLocais[i].origem;
                int v = arestasLocais[i].destino;

                if (raiz[u] != raiz[v]) {
                    arestasLocais[novaQuantidade] = arestasLocais[i];
                    novaQuantidade++;
                }
            }

            nLocais = novaQuantidade;
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);

    /* O tempo medido termina antes da ordenação, soma e escrita do arquivo. */
    double fimAlgoritmo = MPI_Wtime();
    double pesoTotal = 0.0;

    if (rank == 0) {
        qsort(agm, (size_t)qtdAGM, sizeof(Aresta), compararArestasPorPeso);

        for (int i = 0; i < qtdAGM; i++)
            pesoTotal += agm[i].peso;
    }

    if (rank == 0) {
        double tempoTotal = fimAlgoritmo - inicioTempo;

        FILE *saida = fopen("saida_par.txt", "w");

        if (saida == NULL) {
            printf("Erro ao criar saida_par.txt\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        fprintf(saida, "Peso total: %.12f\n", pesoTotal);
        fprintf(saida, "Tempo total: %.6f segundos\n\n", tempoTotal);
        fprintf(saida, "Arestas da AGM:\n");

        for (int i = 0; i < qtdAGM; i++)
            fprintf(saida, "%d %.12f %d\n", agm[i].origem, agm[i].peso, agm[i].destino);

        fclose(saida);

        printf("Peso total: %.12f\n", pesoTotal);
        printf("Tempo total: %.6f segundos\n", tempoTotal);
        printf("Arquivo gerado: saida_par.txt\n");
    }

    free(arestasLocais);
    free(raiz);
    free(altura);
    free(componentesAtivos);
    free(mapaComponente);
    free(novaLista);
    free(marca);
    free(menorLocal);
    free(menorGlobal);
    free(agm);
    free(unioes);

    MPI_Op_free(&op_menor_aresta);
    MPI_Type_free(&MPI_UNIAO);
    MPI_Type_free(&MPI_ARESTA);

    MPI_Finalize();

    return 0;
}
