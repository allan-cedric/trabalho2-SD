/*
  Trabalho pratico 2 da disciplina de Sistemas Distribuidos (CI1088)

  Implementacao do detector de falhas vCube no ambiente de simulação SMPL
  (Versao assincrona com falsa suspeitas)

  Autor: Allan Cedric G. B. Alves da Silva - GRR20190351
*/

#include <stdio.h>
#include <stdlib.h>
#include "smpl.h"
#include "cisj.h"

/* cada processo conta seu tempo */

/*---- aqui definimos os eventos ----*/
#define test 1
#define fault 2
#define recovery 3

// estados dos processos
#define unknown -1
#define correto 0
#define falho 1

/*---- declaramos agora o TipoProcesso ----*/
typedef struct
{
  int id;          /* identificador de facility SMPL */
  int *State;      /* contadores de eventos nos processos */
  int *Tested;     /* processos foram testados */
  int rodada_flag; /* verifica se ja cumpriu a rodada */
  int term;        /* indica que o processo encerrou sua execucao */
                   /* vari�veis locais do processo s�o declaradas aqui */
} TipoProcesso;

// Retorna 1 se esta falho, senao 0
int testa(TipoProcesso p);

// Retorna 1 se a rodada esta completa, senao 0
int rodada_completa(TipoProcesso *ps, int N);

// Retorna 1 se for falsa suspeita, senao 0
int falsa_suspeita(int prob);

TipoProcesso *processo;

int main(int argc, char *argv[])
{
  static int N,     /* number of nodes is parameter */
      PROB,         /* probabilidade de falsa suspeita*/
      token,        /* node identifier, natural number */
      nova_rodada,  /* flag que indica se eh uma nova rodada */
      ini_rodada,   /* salva a rodada inicial apos um evento */
      cont_rodada,  /* contador de rodadas */
      proc_mudou,   /* processo alvo do evento ocorrido */
      num_testes,   /* numero de testes para um certo evento */
      event,
      r,
      i, j;

  int *ini_rodada_arr; /* ini_rodada para cada processo (util para saber a rodada inicial de uma falsa suspeita) */

  static char fa_name[5];

  if (argc != 3)
  {
    puts("Uso correto: vcube <num-processos> <prob-falsa-suspeita-em-%%>");
    exit(1);
  }

  if (atoi(argv[1]) < 2)
  {
    puts("Erro: O sistema precisa de no minimo 2 processos");
    exit(1);
  }

  if (atoi(argv[2]) < 0 || atoi(argv[2]) > 100)
  {
    puts("Erro: Probabilidade deve ser no minimo 0 e no maximo 100");
    exit(1);
  }

  N = atoi(argv[1]);
  PROB = atoi(argv[2]);
  smpl(0, "Um Exemplo de Simula��o");
  reset();
  stream(1);

  printf("Trabalho pratico 2 da disciplina de Sistemas Distribuidos (CI1088)\n\n"
         "Implementacao do detector de falhas vCube no ambiente de simulação SMPL\n"
         "(Versao assincrona com falsa suspeitas)\n\n"
         "Autor: Allan Cedric G. B. Alves da Silva - GRR20190351\n\n");

  printf("Info.: N = %i, PROB = %i%% \n\n", N, PROB);

  /*----- inicializacao -----*/

  processo = (TipoProcesso *)malloc(sizeof(TipoProcesso) * N);
  ini_rodada_arr = (int *)calloc(N, sizeof(int));

  for (i = 0; i < N; i++)
  {
    memset(fa_name, '\0', 5);
    sprintf(fa_name, "%d", i);
    processo[i].id = facility(fa_name, 1);

    processo[i].State = (int *)malloc(sizeof(int) * N);
    for (int j = 0; j < N; j++)
      processo[i].State[j] = unknown;
    processo[i].State[i] = correto;

    processo[i].Tested = (int *)calloc(N, sizeof(int));
    processo[i].rodada_flag = 0;
    processo[i].term = 0;
    // printf("fa_name = %s, processo[%d].id = %d\n", fa_name, i, processo[i].id);
  } /* end for */

  /*----- vamos escalonar os eventos iniciais -----*/

  for (i = 0; i < N; i++)
    schedule(test, 30.0, i);
  // schedule(fault, 58.0, 0);
  // schedule(fault, 88.0, 1);
  // schedule(recovery, 138.0, 0);

  /*----- agora vem o loop principal do simulador -----*/

  nova_rodada = 1;
  ini_rodada = -1; // garante que as rotinas de analise de evento nao sejam disparadas
  cont_rodada = 1;
  while (time() <= 150.0)
  {
    cause(&event, &token);
    switch (event)
    {
      case test:

        // inicio de uma rodada
        if (nova_rodada)
        {
          printf("\n**************** RODADA %i ****************\n\n", cont_rodada);
          nova_rodada = 0;
        }

        if (testa(processo[token])) // processo falho n�o testa!
          break;

        // vcube
        int num_max_falhas = 0;
        int s_max = log2((double)N); // num. maximo de clusters

        // para cada cluster s
        for(int s = 1; s <= s_max; s++)
        {
          // para cada processo j de C(token,s)
          node_set *lista_cis = cis(token, s);
          for(int j = 0; j < lista_cis->size; j++)
          {
            // verifique se existe um processo i tal que seja o primeiro nodo correto de C(j, s)
            int pi = -1;
            int pj = lista_cis->nodes[j];
            node_set *lista_cjs = cis(pj, s);
            for(int k = 0; k < lista_cjs->size; k++)
            {
              // primeiro processo correto que encontrar
              if(!testa(processo[lista_cjs->nodes[k]]))
              {
                pi = lista_cjs->nodes[k];
                break;
              }
            }

            // processo i deve ser exatamente o processo atual
            if(token == pi)
            {
              int fs = 0; // indica falsa suspeita
              if(testa(processo[pj]) || (fs = falsa_suspeita(PROB))) // processo j falho ou falsa suspeita
              {
                // falsa suspeita de pj e rodada inicial de falsa suspeita
                if(fs && ini_rodada_arr[pj] == 0)
                  ini_rodada_arr[pj] = cont_rodada;

                printf("o processo %i testou o processo %i FALHO no tempo %5.1f", token, pj, time());

                if(fs)
                  printf(" <falsa_suspeita>\n");
                else
                  printf("\n");

                if(processo[token].State[pj] == unknown)
                  processo[token].State[pj] = 1;
                else if(processo[token].State[pj] % 2 == 0)
                  processo[token].State[pj]++;

                processo[token].Tested[pj] = 1;

                num_max_falhas++;
              }
              else
              {
                printf("o processo %i testou o processo %i CORRETO no tempo %5.1f\n", token, pj, time());

                // processo testador token descobre que foi vitima de falsa suspeita, entao encerra
                if(processo[pj].State[token] % 2 == 1 && processo[pj].State[token] != unknown) {
                  r = request(processo[token].id, token, 0);
                  processo[token].term = 1;
                  printf("o processo %i encerrou sua EXECUCAO no tempo %5.1f\n", token, time());
                  printf("--- NUM. RODADAS: %i ---\n\n", cont_rodada - ini_rodada_arr[token] + 1);
                }
                else {
                  if(processo[token].State[pj] % 2 == 1 || processo[token].State[pj] == unknown)
                    processo[token].State[pj]++;

                  processo[token].Tested[pj] = 1;
                  processo[token].rodada_flag = 1;

                  // info. dos outros processos eh atualizada
                  for(int k = 0; k < N; k++)
                  {
                    if(processo[pj].State[k] > processo[token].State[k])
                      processo[token].State[k] = processo[pj].State[k];
                  }
                }
              }
              if(ini_rodada > 0)
                num_testes++;
            }
            free(lista_cjs->nodes);
            free(lista_cjs);

            if(processo[token].term)
              break;
          }
          free(lista_cis->nodes);
          free(lista_cis);

          if(processo[token].term)
              break;
        }
        // fim vcube

        // processo nao vai encerrar ?
        if(!processo[token].term) {
          if(num_max_falhas == N-1)
            processo[token].rodada_flag = 1;

          // vetor State
          printf("State_%i: {%i", token, processo[token].State[0]);
          for (i = 1; i < N; i++)
            printf(", %i", processo[token].State[i]);
          printf("}\n\n");
        }

        // fim de rodada
        if (rodada_completa(processo, N))
        {
          if(ini_rodada > 0)
          {
            printf("**************** FIM DA RODADA %i ****************\n", cont_rodada);

            int diag_completo = 1;
            int estado_novo = testa(processo[proc_mudou]) % 2; // paridade do evento
            for(i = 0; i < N; i++)
            {
              // processo i diferente do processo alvo do evento e que esteja correto
              if(i != proc_mudou && !testa(processo[i]))
              {
                // processo i nao sabe do evento ?
                if(processo[i].State[proc_mudou] % 2 != estado_novo)
                {
                  diag_completo = 0;
                  break;
                }
              }
            }

            if(diag_completo)
            {
              printf("==================================================\n");
              printf("Resultado do diagnostico (evento no processo %i):\n", proc_mudou);
              printf("Num. de testes: %i\n", num_testes);
              printf("Latencia: %i\n", cont_rodada - ini_rodada + 1);
              printf("==================================================\n\n\n");
              ini_rodada = -1;
            }
          }
          else
            printf("**************** FIM DA RODADA %i ****************\n\n\n", cont_rodada);

          for (i = 0; i < N; i++)
          {
            if (!testa(processo[i]))
            {
              memset(processo[i].Tested, 0, sizeof(int) * N);
              processo[i].rodada_flag = 0;
            }
          }
          cont_rodada++;
          nova_rodada = 1;
        }

        // processo nao vai encerrar ?
        if(!processo[token].term)
          schedule(test, 30.0, token);
        break;
      case fault:
        r = request(processo[token].id, token, 0);
        printf("EVENTO: o processo %d falhou no tempo %5.1f\n\n", token, time());

        // dispara rotinas para analise de eventos
        // ini_rodada = cont_rodada;
        // proc_mudou = token;
        // num_testes = 0;
        break;
      case recovery:
        release(processo[token].id, token);
        schedule(test, 30.0, token);
        printf("EVENTO: o processo %d recuperou no tempo %5.1f\n\n", token, time());

        // dispara rotinas para analise de eventos
        // ini_rodada = cont_rodada;
        // proc_mudou = token;
        // num_testes = 0;
        break;
    } /* end switch */
  } /* end while */

  for(i = 0; i < N; i++)
  {
    free(processo[i].State);
    free(processo[i].Tested);
  }
  free(processo);

  return 0;
} /* end tempo.c */

int testa(TipoProcesso p)
{
  return status(p.id) != 0;
}

int rodada_completa(TipoProcesso *ps, int N)
{
  for(int i = 0; i < N; i++)
  {
    if(!testa(ps[i]) && !ps[i].rodada_flag)
      return 0;
  }
  return 1;
}

int falsa_suspeita(int prob)
{
  return randomic(1, 100) <= prob;
}
