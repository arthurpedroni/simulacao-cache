#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#define TEMPO_LRU_PADRAO 4

typedef struct {
    int valido;
    int dirty;
    int lru;
    uint32_t rotulo;
} LinhaCache;

typedef struct {
    int escrita;
    int leitura;
} Contador;

Contador cnt_arquivo = {0, 0};
Contador cnt_memoria = {0, 0};
Contador cnt_acerto  = {0, 0};
Contador cnt_falha   = {0, 0};

int potenciaDeDois(int n) {
    if(n == 0) return 0;
    while(n != 1) {
        if(n % 2 != 0) return 0;
        n = n / 2;
    }
    return 1;
}

LinhaCache **alocarCache(int conjuntos, int assoc) {
    int i, j;
    LinhaCache **c = (LinhaCache**) malloc(conjuntos * sizeof(LinhaCache*));
    for(i = 0; i < conjuntos; i++) {
        c[i] = (LinhaCache*) malloc(assoc * sizeof(LinhaCache));
        for(j = 0; j < assoc; j++) {
            c[i][j].valido = 0;
            c[i][j].dirty  = 0;
            c[i][j].lru    = TEMPO_LRU_PADRAO;
            c[i][j].rotulo = 0;
        }
    }
    return c;
}

void liberarCache(LinhaCache **c, int conjuntos) {
    int i;
    for(i = 0; i < conjuntos; i++) free(c[i]);
    free(c);
}

void extrairCampos(uint32_t endereco, int bitsOffset, int bitsConj,
                   int *idx, uint32_t *rotulo) {
    *idx    = (endereco >> bitsOffset) & ((1 << bitsConj) - 1);
    *rotulo = (endereco >> bitsOffset) >> bitsConj;
}

int encontrarVitima(LinhaCache **c, int idx, int assoc, char *pol_subst) {
    int i, vitima;
    vitima = 0;
    if(strcmp(pol_subst, "LRU") == 0) {
        for(i = 1; i < assoc; i++)
            if(c[idx][i].lru < c[idx][vitima].lru)
                vitima = i;
    } else {
        vitima = rand() % assoc;
    }
    return vitima;
}

int lerCache(LinhaCache **c, uint32_t endereco, int bitsOffset, int bitsConj, int assoc) {
    int i, idx;
    uint32_t rotulo;
    extrairCampos(endereco, bitsOffset, bitsConj, &idx, &rotulo);

    for(i = 0; i < assoc; i++) {
        if(c[idx][i].valido && c[idx][i].rotulo == rotulo) {
            cnt_acerto.leitura++;
            c[idx][i].lru = TEMPO_LRU_PADRAO;
            return 1;
        }
    }
    cnt_falha.leitura++;
    return 0;
}

void carregarBloco(LinhaCache **c, uint32_t endereco, int bitsOffset, int bitsConj, char *pol_subst, int assoc) {
    int i, idx, vitima;
    uint32_t rotulo;
    extrairCampos(endereco, bitsOffset, bitsConj, &idx, &rotulo);

    cnt_memoria.leitura++;

    for(i = 0; i < assoc; i++) {
        if(!c[idx][i].valido) {
            c[idx][i].valido = 1;
            c[idx][i].dirty  = 0;
            c[idx][i].rotulo = rotulo;
            c[idx][i].lru    = TEMPO_LRU_PADRAO;
            return;
        }
    }

    vitima = encontrarVitima(c, idx, assoc, pol_subst);

    if(c[idx][vitima].dirty)
        cnt_memoria.escrita++;

    c[idx][vitima].valido = 1;
    c[idx][vitima].dirty  = 0;
    c[idx][vitima].rotulo = rotulo;
    c[idx][vitima].lru    = TEMPO_LRU_PADRAO;
}

int escreverCache(LinhaCache **c, uint32_t endereco, int bitsOffset, int bitsConj, int pol_escrita, char *pol_subst, int assoc) {
    int i, idx, vitima;
    uint32_t rotulo;
    extrairCampos(endereco, bitsOffset, bitsConj, &idx, &rotulo);

    for(i = 0; i < assoc; i++) {
        if(c[idx][i].valido && c[idx][i].rotulo == rotulo) {
            c[idx][i].lru = TEMPO_LRU_PADRAO;
            if(pol_escrita == 0) {
                cnt_memoria.escrita++;
            } else {
                c[idx][i].dirty = 1;
            }
            return 1;
        }
    }

    if(pol_escrita == 0) {
        cnt_memoria.escrita++;
        return 0;
    }

    cnt_memoria.leitura++;

    for(i = 0; i < assoc; i++) {
        if(!c[idx][i].valido) {
            c[idx][i].valido = 1;
            c[idx][i].dirty  = 1;
            c[idx][i].rotulo = rotulo;
            c[idx][i].lru    = TEMPO_LRU_PADRAO;
            return 0;
        }
    }

    vitima = encontrarVitima(c, idx, assoc, pol_subst);

    if(c[idx][vitima].dirty)
        cnt_memoria.escrita++;

    c[idx][vitima].valido = 1;
    c[idx][vitima].dirty  = 1;
    c[idx][vitima].rotulo = rotulo;
    c[idx][vitima].lru    = TEMPO_LRU_PADRAO;
    return 0;
}

void decrementarLRU(LinhaCache **c, int conjuntos, int assoc) {
    int i, j;
    for(i = 0; i < conjuntos; i++)
        for(j = 0; j < assoc; j++)
            if(c[i][j].lru > 0)
                c[i][j].lru--;
}

void flushCache(LinhaCache **c, int conjuntos, int assoc) {
    int i, j;
    for(i = 0; i < conjuntos; i++)
        for(j = 0; j < assoc; j++)
            if(c[i][j].valido && c[i][j].dirty)
                cnt_memoria.escrita++;
}

int main(int argc, char *argv[]) {
    char *arq_entrada;
    char *arq_saida;
    char *pol_subst;
    int pol_escrita, tam_bloco, num_blocos, assoc, tempo_acerto, tempo_mem;
    int conjuntos, bitsOffset, bitsConj;
    int tot_w, tot_r, tot;
    float hr_w, hr_r, hr_t, tempo_medio;
    uint32_t endereco;
    char operacao;
    FILE *arq;
    LinhaCache **cache;
    
    if(argc != 10) {
        printf("Parametros invalidos\n");
        printf("simulador_cache.exe <arq_entrada> <arq_saida> <pol_escrita> <tam_bloco> <num_blocos> <assoc> <tempo_acerto> <pol_subst> <tempo_mem>\n");
        return 1;
    }

    arq_entrada  = argv[1];
    arq_saida = argv[2];
    pol_escrita = atoi(argv[3]);
    tam_bloco = atoi(argv[4]);
    num_blocos = atoi(argv[5]);
    assoc = atoi(argv[6]);
    tempo_acerto = atoi(argv[7]);
    pol_subst = argv[8];
    tempo_mem = atoi(argv[9]);

    if(!potenciaDeDois(tam_bloco) || !potenciaDeDois(num_blocos) || assoc < 1 || assoc > num_blocos || (assoc > 1 && !potenciaDeDois(assoc))) {
        printf("Tamanho do bloco ou associatividade invalidos.\n");
        return 1;
    }

    conjuntos = num_blocos / assoc;
    bitsOffset = (int)(log((float)tam_bloco) / log(2.0) + 0.5);
    bitsConj = (int)(log((float)conjuntos) / log(2.0) + 0.5);

    arq = fopen(arq_entrada, "r");
    if(arq == NULL) {
        printf("Erro ao abrir arquivo de entrada.\n");
        return 1;
    }

    srand((unsigned)time(NULL));
    cache = alocarCache(conjuntos, assoc);

    while(fscanf(arq, "%x %c", &endereco, &operacao) == 2) {
        if(operacao == 'R') {
            cnt_arquivo.leitura++;
            if(!lerCache(cache, endereco, bitsOffset, bitsConj, assoc))
                carregarBloco(cache, endereco, bitsOffset, bitsConj, pol_subst, assoc);
        } else if(operacao == 'W') {
            cnt_arquivo.escrita++;
            if(escreverCache(cache, endereco, bitsOffset, bitsConj, pol_escrita, pol_subst, assoc))
                cnt_acerto.escrita++;
            else
                cnt_falha.escrita++;
        }
        if(strcmp(pol_subst, "LRU") == 0)
            decrementarLRU(cache, conjuntos, assoc);
    }
    fclose(arq);

    flushCache(cache, conjuntos, assoc);

    arq = fopen(arq_saida, "a");
    if(arq == NULL) {
        printf("Erro ao abrir arquivo de saida.\n");
        return 1;
    }

    fprintf(arq, "Arquivo de entrada: %s\n", arq_entrada);

    if(pol_escrita == 0) {
        fprintf(arq, "Politica de escrita: write-through\n");
    } else {
        fprintf(arq, "Politica de escrita: write-back\n");
    }

    fprintf(arq, "Tamanho da linha: %d bytes\n", tam_bloco);
    fprintf(arq, "Numero de linhas: %d\n", num_blocos);
    fprintf(arq, "Associatividade: %d\n", assoc);
    fprintf(arq, "Hit time: %d ns\n", tempo_acerto);

    if(strcmp(pol_subst, "LRU") == 0) {
        fprintf(arq, "Politica de substituicao: LRU\n");
    } else {
        fprintf(arq, "Politica de substituicao: Aleatoria\n");
    }

    fprintf(arq, "Tempo leitura/escrita: %d ns\n\n", tempo_mem);

    fprintf(arq, "Total de enderecos:\n");
    fprintf(arq, "  Escrita: %d\n", cnt_arquivo.escrita);
    fprintf(arq, "  Leitura: %d\n", cnt_arquivo.leitura);
    fprintf(arq, "  Total: %d\n\n", cnt_arquivo.escrita + cnt_arquivo.leitura);

    fprintf(arq, "Memoria principal:\n");
    fprintf(arq, "  Escritas: %d\n", cnt_memoria.escrita);
    fprintf(arq, "  Leituras: %d\n", cnt_memoria.leitura);
    fprintf(arq, "  Total: %d\n\n", cnt_memoria.escrita + cnt_memoria.leitura);

    tot_w = cnt_acerto.escrita + cnt_falha.escrita;
    tot_r = cnt_acerto.leitura + cnt_falha.leitura;
    tot   = tot_w + tot_r;

    if(tot_w) {
        hr_w = (float)cnt_acerto.escrita / tot_w;
    } else {
        hr_w = 0.0f;
    }

    if(tot_r) {
        hr_r = (float)cnt_acerto.leitura / tot_r;
    } else {
        hr_r = 0.0f;
    }

    if(tot) {
        hr_t = (float)(cnt_acerto.escrita + cnt_acerto.leitura) / tot;
    } else {
        hr_t = 0.0f;
    }

    fprintf(arq, "Taxa de acerto (hit rate):\n");
    fprintf(arq, "  Escrita: %.4f%% (%d acertos de %d)\n", hr_w*100, cnt_acerto.escrita, tot_w);
    fprintf(arq, "  Leitura: %.4f%% (%d acertos de %d)\n", hr_r*100, cnt_acerto.leitura, tot_r);
    fprintf(arq, "  Global: %.4f%% (%d acertos de %d)\n\n", hr_t*100,
            cnt_acerto.escrita + cnt_acerto.leitura, tot);

    tempo_medio = hr_t * tempo_acerto + (1.0f - hr_t) * (tempo_acerto + tempo_mem);
    fprintf(arq, "Tempo medio de acesso: %.4f ns\n", tempo_medio);
    fprintf(arq, "============================================================\n\n");

    fclose(arq);
    liberarCache(cache, conjuntos);
    printf("Simulador cache finalizado com sucesso\n");
    return 0;
}
