/*
  war.c
  Exemplo de sistema simplificado inspirado em War (jogo de tabuleiro)
  - Cadastro de territórios (nome, cor, tropas) usando vetor de structs
  - Ataque entre territórios usando alocação dinâmica e ponteiros
  - Sistema de missões alocado dinamicamente
  Observação: código educacional — não implementa todas regras do War real.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#define MAX_PLAYERS 6
#define MAX_DICE 3

/* ----- Estruturas ----- */

typedef struct {
    char *name;      // nome do território
    char *color;     // cor/ocupante (string representando jogador ou cor)
    int troops;      // número de tropas
    int owner_id;    // id do jogador dono (-1 se neutro)
} Territory;

typedef struct {
    int id;
    char *name;
} Player;

typedef enum {
    MISSION_CONQUER_TERRITORIES,
    MISSION_ELIMINATE_PLAYER,
    MISSION_HAVE_TROOPS_TOTAL
} MissionType;

typedef struct {
    char *description;
    MissionType type;
    // parâmetros variáveis para cada tipo de missão
    int target_player_id;   // usado por MISSION_ELIMINATE_PLAYER
    int required_count;     // número de territórios ou tropas dependendo do type
    bool completed;
} Mission;

/* ----- Helpers de string e memória ----- */

char *strdup_local(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ----- Funções para territórios ----- */

Territory *create_territories(int n) {
    Territory *arr = malloc(sizeof(Territory) * n);
    if (!arr) {
        perror("malloc territories");
        return NULL;
    }
    for (int i = 0; i < n; ++i) {
        arr[i].name = NULL;
        arr[i].color = NULL;
        arr[i].troops = 0;
        arr[i].owner_id = -1;
    }
    return arr;
}

void free_territories(Territory *arr, int n) {
    if (!arr) return;
    for (int i = 0; i < n; ++i) {
        free(arr[i].name);
        free(arr[i].color);
    }
    free(arr);
}

void set_territory(Territory *t, const char *name, const char *color, int troops, int owner_id) {
    if (!t) return;
    free(t->name);
    free(t->color);
    t->name = strdup_local(name);
    t->color = strdup_local(color);
    t->troops = troops > 0 ? troops : 0;
    t->owner_id = owner_id;
}

void print_territories(Territory *arr, int n) {
    printf("Lista de Territórios:\n");
    for (int i = 0; i < n; ++i) {
        printf(" [%d] %s | Cor: %s | Tropas: %d | Dono: %d\n",
               i, arr[i].name ? arr[i].name : "(sem nome)",
               arr[i].color ? arr[i].color : "(sem cor)",
               arr[i].troops, arr[i].owner_id);
    }
}

/* ----- Funções para jogadores ----- */

Player *create_players(int n) {
    Player *p = malloc(sizeof(Player) * n);
    if (!p) {
        perror("malloc players");
        return NULL;
    }
    for (int i = 0; i < n; ++i) {
        p[i].id = i;
        p[i].name = NULL;
    }
    return p;
}

void free_players(Player *p, int n) {
    if (!p) return;
    for (int i = 0; i < n; ++i) free(p[i].name);
    free(p);
}

void set_player(Player *p, int id, const char *name) {
    p[id].id = id;
    free(p[id].name);
    p[id].name = strdup_local(name);
}

/* ----- Funções para missões ----- */

Mission *create_missions(int count) {
    Mission *m = malloc(sizeof(Mission) * count);
    if (!m) {
        perror("malloc missions");
        return NULL;
    }
    for (int i = 0; i < count; ++i) {
        m[i].description = NULL;
        m[i].completed = false;
        m[i].type = MISSION_CONQUER_TERRITORIES;
        m[i].target_player_id = -1;
        m[i].required_count = 0;
    }
    return m;
}

void free_missions(Mission *m, int count) {
    if (!m) return;
    for (int i = 0; i < count; ++i) free(m[i].description);
    free(m);
}

/* ----- Sistema de dados/ataque ----- */

static void swap_int(int *a, int *b) { int t = *a; *a = *b; *b = t; }

/* ordena desc uma pequena array (bubble suficiente para <=3 elementos) */
static void sort_desc(int arr[], int n) {
    for (int i = 0; i < n-1; ++i)
        for (int j = 0; j < n-1-i; ++j)
            if (arr[j] < arr[j+1]) swap_int(&arr[j], &arr[j+1]);
}

/* rola um dado (1..6) */
static int roll_die(void) {
    return (rand() % 6) + 1;
}

/*
  resolve_attack:
    - attacker_idx: índice do território atacante no array
    - defender_idx: índice do território defensor no array
    - terr: ponteiro para array de territórios
    - n: número total de territórios (apenas para validação)
    - max_attacker_dice / max_defender_dice: seguem regras do War clássico (attacker: até 3, defender: até 2)
  Retorna:
    0: ataque inválido (mesmo dono, tropas insuficientes, índices inválidos)
    1: ataque realizado, defensor ainda resiste
    2: atacante conquistou o território (troca de dono e transferência mínima de tropas)
*/
int resolve_attack(int attacker_idx, int defender_idx, Territory *terr, int n, int max_attacker_dice, int max_defender_dice) {
    if (!terr) return 0;
    if (attacker_idx < 0 || attacker_idx >= n || defender_idx < 0 || defender_idx >= n) return 0;
    Territory *A = &terr[attacker_idx];
    Territory *D = &terr[defender_idx];
    if (A->owner_id == D->owner_id) { printf("Ataque inválido: mesmo dono.\n"); return 0; }
    if (A->troops < 2) { printf("Ataque inválido: tropas insuficientes em %s (necessita >1).\n", A->name); return 0; }

    int attacker_dice = A->troops - 1; // pode arriscar até tropas-1
    if (attacker_dice > max_attacker_dice) attacker_dice = max_attacker_dice;
    if (attacker_dice < 1) { printf("Ataque inválido: attacker_dice<1\n"); return 0; }

    int defender_dice = D->troops;
    if (defender_dice > max_defender_dice) defender_dice = max_defender_dice;
    if (defender_dice < 1) defender_dice = 1;

    int adice[3] = {0,0,0};
    int ddice[2] = {0,0};

    for (int i = 0; i < attacker_dice; ++i) adice[i] = roll_die();
    for (int i = 0; i < defender_dice; ++i) ddice[i] = roll_die();

    sort_desc(adice, attacker_dice);
    sort_desc(ddice, defender_dice);

    printf("Ataque: %s (%d tropas) -> %s (%d tropas)\n", A->name, A->troops, D->name, D->troops);
    printf("Dados atacante: ");
    for (int i = 0; i < attacker_dice; ++i) printf("%d ", adice[i]);
    printf("\nDados defensor: ");
    for (int i = 0; i < defender_dice; ++i) printf("%d ", ddice[i]);
    printf("\n");

    int comparisons = attacker_dice < defender_dice ? attacker_dice : defender_dice;
    int attacker_losses = 0;
    int defender_losses = 0;

    for (int i = 0; i < comparisons; ++i) {
        if (adice[i] > ddice[i]) {
            defender_losses++;
        } else {
            attacker_losses++;
        }
    }

    A->troops -= attacker_losses;
    D->troops -= defender_losses;

    printf("Resultado: atacante perde %d tropa(s), defensor perde %d tropa(s).\n", attacker_losses, defender_losses);
    if (D->troops <= 0) {
        printf("%s foi conquistado por jogador %d!\n", D->name, A->owner_id);
        D->owner_id = A->owner_id;
        // transferir pelo menos 1 tropa (ou o número de dados atacante) do atacante para o conquistado
        int transfer = attacker_dice; // estratégia simples
        if (transfer >= A->troops) transfer = A->troops - 1; // deixa pelo menos 1 no atacador
        if (transfer < 1) transfer = 1;
        A->troops -= transfer;
        D->troops = transfer;
        printf("%d tropa(s) movidas de %s para %s durante a conquista.\n", transfer, A->name, D->name);
        return 2;
    }
    return 1;
}

/* ----- Funções de verificação de missão ----- */

int count_territories_of_player(Territory *arr, int n, int player_id) {
    int c = 0;
    for (int i = 0; i < n; ++i)
        if (arr[i].owner_id == player_id) ++c;
    return c;
}

int total_troops_of_player(Territory *arr, int n, int player_id) {
    int t = 0;
    for (int i = 0; i < n; ++i)
        if (arr[i].owner_id == player_id) t += arr[i].troops;
    return t;
}

bool is_player_eliminated(Territory *arr, int n, int player_id) {
    for (int i = 0; i < n; ++i)
        if (arr[i].owner_id == player_id) return false;
    return true;
}

/* Atualiza o estado de conclusão da missão e retorna true se completada agora */
bool check_and_update_mission(Mission *m, Territory *arr, int n, Player *players, int player_count, int mission_owner_id) {
    if (!m) return false;
    if (m->completed) return true;

    switch (m->type) {
    case MISSION_CONQUER_TERRITORIES: {
        int owned = count_territories_of_player(arr, n, mission_owner_id);
        if (owned >= m->required_count) {
            m->completed = true;
            return true;
        }
        break;
    }
    case MISSION_ELIMINATE_PLAYER: {
        if (m->target_player_id < 0 || m->target_player_id >= player_count) break;
        if (is_player_eliminated(arr, n, m->target_player_id)) {
            m->completed = true;
            return true;
        }
        break;
    }
    case MISSION_HAVE_TROOPS_TOTAL: {
        int troops = total_troops_of_player(arr, n, mission_owner_id);
        if (troops >= m->required_count) {
            m->completed = true;
            return true;
        }
        break;
    }
    default:
        break;
    }
    return false;
}

/* ----- Utilitários e demonstração ----- */

void print_missions(Mission *m, int count, int owner_id) {
    printf("Missões do jogador %d:\n", owner_id);
    for (int i = 0; i < count; ++i) {
        printf(" [%d] %s | Tipo: %d | Req: %d | Alvo: %d | Concluida: %s\n",
            i,
            m[i].description ? m[i].description : "(sem descrição)",
            (int)m[i].type,
            m[i].required_count,
            m[i].target_player_id,
            m[i].completed ? "SIM" : "NAO");
    }
}

/* Função de demonstração que configura um pequeno cenário */
void demo() {
    srand((unsigned)time(NULL));

    int territory_count = 6;
    Territory *terr = create_territories(territory_count);

    int player_count = 3;
    Player *players = create_players(player_count);
    set_player(players, 0, "Alice");
    set_player(players, 1, "Bob");
    set_player(players, 2, "Carol");

    // configurar territórios (nome, cor, tropas, owner)
    set_territory(&terr[0], "Brasil", "Vermelho", 5, 0);
    set_territory(&terr[1], "Argentina", "Vermelho", 3, 0);
    set_territory(&terr[2], "EUA", "Azul", 6, 1);
    set_territory(&terr[3], "Canada", "Azul", 2, 1);
    set_territory(&terr[4], "China", "Verde", 4, 2);
    set_territory(&terr[5], "India", "Verde", 3, 2);

    print_territories(terr, territory_count);

    // criar missões para cada jogador (exemplo)
    int mission_count = 2;
    Mission *mA = create_missions(mission_count);
    mA[0].description = strdup_local("Conquistar 3 territorios");
    mA[0].type = MISSION_CONQUER_TERRITORIES;
    mA[0].required_count = 3;

    mA[1].description = strdup_local("Eliminar o jogador 2 (Carol)");
    mA[1].type = MISSION_ELIMINATE_PLAYER;
    mA[1].target_player_id = 2;

    print_missions(mA, mission_count, 0);

    // simular ataque: Alice (Brasil idx 0) ataca EUA (idx 2)
    printf("\n-- Rodada de ataque 1 --\n");
    resolve_attack(0, 2, terr, territory_count, 3, 2);
    print_territories(terr, territory_count);

    // simular ataque: Bob (EUA/Canada) ataca Brasil
    printf("\n-- Rodada de ataque 2 --\n");
    resolve_attack(2, 0, terr, territory_count, 3, 2);
    print_territories(terr, territory_count);

    // verificar missões de Alice:
    for (int i = 0; i < mission_count; ++i) {
        bool completed = check_and_update_mission(&mA[i], terr, territory_count, players, player_count, 0);
        if (completed) printf("Missão %d concluída!\n", i);
    }
    print_missions(mA, mission_count, 0);

    // limpeza
    free_missions(mA, mission_count);
    free_players(players, player_count);
    free_territories(terr, territory_count);
}

/* ----- MAIN ----- */

int main(void) {
    printf("=== Sistema simplificado de War (demo) ===\n\n");
    demo();
    printf("\nDemo finalizada.\n");
    return 0;
}
