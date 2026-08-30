/*
AUTOMAÇÃO EM TEMPO REAL
Jogo da Velha com Programação Concorrente
Victor Luiz Lima Rodrigues - 2023038515
*/

#include <iostream>
#include <random>
#include <thread>
#include <array>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstdlib>
#include <string>

class TicTacToe {
private:
    std::array<std::array<char, 3>, 3> board;
    char current_player;
    bool game_over;
    char winner;

    // O mutex garante acesso exclusivo ao estado compartilhado do jogo.
    std::mutex game_mutex;

    // A variável de condição faz cada jogador esperar a própria vez.
    std::condition_variable turn_condition;

    // Esta versão é usada quando o mutex já está travado.
    void display_board_unlocked() {
#ifdef _WIN32
        std::system("cls");
#else
        std::system("clear");
#endif

        for (int i = 0; i < 3; ++i) {
            std::cout << board[i][0] << "|"
                      << board[i][1] << "|"
                      << board[i][2] << std::endl;

            if (i != 2) {
                std::cout << "-----" << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    bool check_win_unlocked(char player) {
        // Linhas e colunas
        for (int i = 0; i < 3; ++i) {
            if (board[i][0] == player &&
                board[i][1] == player &&
                board[i][2] == player) {
                return true;
            }

            if (board[0][i] == player &&
                board[1][i] == player &&
                board[2][i] == player) {
                return true;
            }
        }

        // Diagonais
        if (board[0][0] == player &&
            board[1][1] == player &&
            board[2][2] == player) {
            return true;
        }

        if (board[0][2] == player &&
            board[1][1] == player &&
            board[2][0] == player) {
            return true;
        }

        return false;
    }

    bool check_draw_unlocked() {
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                if (board[i][j] == ' ') {
                    return false;
                }
            }
        }

        return true;
    }

public:
    TicTacToe()
        : game_over(false), winner('-') {

        for (int i = 0; i < 3; ++i) {
            board[i].fill(' ');
        }

        // Sorteia quem começa.
        std::mt19937 generator(
            static_cast<unsigned int>(
                std::chrono::steady_clock::now()
                    .time_since_epoch()
                    .count()
            )
        );

        std::uniform_int_distribution<int> distribution(0, 1);
        current_player = (distribution(generator) == 0) ? 'X' : 'O';
    }

    void display_board() {
        std::lock_guard<std::mutex> lock(game_mutex);
        display_board_unlocked();
    }

    bool make_move(char player, int row, int col) {
        std::unique_lock<std::mutex> lock(game_mutex);

        // A thread fica bloqueada até chegar a vez do seu jogador
        // ou até o jogo terminar.
        turn_condition.wait(
            lock,
            [this, player]() {
                return game_over || current_player == player;
            }
        );

        if (game_over) {
            return true;
        }

        // Proteção contra coordenadas inválidas.
        if (row < 0 || row >= 3 || col < 0 || col >= 3) {
            return false;
        }

        // Posição já ocupada: o mesmo jogador deve tentar outra.
        if (board[row][col] != ' ') {
            return false;
        }

        // Seção crítica: somente uma thread chega aqui por vez.
        board[row][col] = player;

        display_board_unlocked();

        if (check_win_unlocked(player)) {
            winner = player;
            game_over = true;
        }
        else if (check_draw_unlocked()) {
            winner = 'D';
            game_over = true;
        }
        else {
            // Passa o turno para o outro jogador.
            current_player = (player == 'X') ? 'O' : 'X';
        }

        // Libera o mutex antes de acordar a outra thread.
        lock.unlock();
        turn_condition.notify_all();

        return true;
    }

    bool is_game_over() {
        std::lock_guard<std::mutex> lock(game_mutex);
        return game_over;
    }

    char get_winner() {
        std::lock_guard<std::mutex> lock(game_mutex);
        return winner;
    }
};

class Player {
private:
    TicTacToe& game;
    char symbol;
    std::string strategy;

    void play_sequential() {
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                if (game.make_move(symbol, i, j)) {
                    return;
                }
            }
        }
    }

    void play_random() {
        static thread_local std::mt19937 generator(
            std::random_device{}()
        );

        std::uniform_int_distribution<int> distribution(0, 2);

        while (!game.is_game_over()) {
            int row = distribution(generator);
            int col = distribution(generator);

            if (game.make_move(symbol, row, col)) {
                return;
            }
        }
    }

public:
    Player(
        TicTacToe& g,
        char s,
        const std::string& strat
    )
        : game(g),
          symbol(s),
          strategy(strat) {
    }

    void play() {
        while (!game.is_game_over()) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(200)
            );

            if (strategy == "sequential") {
                play_sequential();
            }
            else {
                play_random();
            }
        }
    }
};

int main() {
    TicTacToe game;

    game.display_board();

    Player player_x(game, 'X', "sequential");
    Player player_o(game, 'O', "random");

    // Uma thread para cada jogador.
    std::thread thread_x(&Player::play, &player_x);
    std::thread thread_o(&Player::play, &player_o);

    thread_x.join();
    thread_o.join();

    char winner = game.get_winner();

    if (winner == 'D') {
        std::cout << "Empate!" << std::endl;
    }
    else {
        std::cout << "Vencedor: " << winner << std::endl;
    }

    return 0;
}
