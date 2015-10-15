#include <iostream>
#include "TaskScheduler.hpp"
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <string>

static float matrix[100][100];

void normalize_row(int row) {
	float sum = 0.0f;

	std::cout << ("Normalizing row " + std::to_string(row) + "\n");

	for(int i = 0; i < 100; i++) {
		sum += (matrix[row][i] * matrix[row][i]);
	}

	float mag = std::sqrt(sum);

	for(int i = 0; i < 100; i++) {
		matrix[row][i] /= mag;
	}
}

float row_sum(int row) {
	float sum = 0.0f;

	std::cout << ("Summing row " + std::to_string(row) + "\n");

	for(int i = 0; i < 100; i++) {
		sum += matrix[row][i];
	}

	return sum;
}

float matrix_sum(geiger::async::TaskScheduler& ts) {
	std::future<float> sums[100];
	float mat_sum = 0.0f;

	for(int i = 0; i < 100; i++) {
		sums[i] = ts.SubmitTask(row_sum, i);
	}

	for(int i = 0; i < 100; i++) {
        mat_sum += sums[i].get();
	}

	return mat_sum;
}

int main() {

	using namespace geiger::async;

	srand(time(NULL));

	TaskScheduler scheduler;

	for(int i = 0; i < 100; i++) {
		scheduler.SubmitTask([=]() {
			for(int j = 0; j < 100; j++) {
				matrix[i][j] = (((float)(rand()) / (float)(RAND_MAX / 2)) - (float)(RAND_MAX / 2)) * 16.0f;
			}
		});
	}

	float random_mat_sum = matrix_sum(scheduler);

	std::cout << "Sum of all elements in a random matrix: " << random_mat_sum << "\n";

	for(int i = 0; i < 100; i++) {
		scheduler.SubmitTask([=]() {
			normalize_row(i);
		});
	}

	float normalized_mat_sum = matrix_sum(scheduler);

	std::cout << "Sum of all normalized rows in a random matrix: " << normalized_mat_sum << "\n";

    return 0;
}
