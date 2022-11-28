// Решите загадку: Сколько чисел от 1 до 1000 содержат как минимум одну цифру 3?
// Напишите ответ здесь:
#include <iostream>
using namespace std;

bool check(int number) {
    while (number > 0) {
        int last_digit = number % 10;
        if (last_digit == 3) {
            return true;
        }
        number = number / 10;
    }
    return false;
}

int main() {
    int count = 0;
    for (int i = 1; i <= 1000; ++i) {
        if (check(i)) {
            ++count;
        }
    }
    cout << count << endl;
}

// Закомитьте изменения и отправьте их в свой репозиторий.
