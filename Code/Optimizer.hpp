#pragma once

#include <map>
#include <vector>
#include <deque>
#include <algorithm>

#include "Types.hpp"


enum class OptimizerMode {
	MINIMIZE,
	MAXIMIZE
};

template<typename Worker, typename Job>
struct Optimizer {
public:
	struct OptimizerResult {
		std::map<Worker, Job> assignments;
		long long int total_value = 0;
	};

	void InsertEdge(Worker w, Job j, long long int value) {
		int w_index = InsertAndGetIndex(workers, w);
		int j_index = InsertAndGetIndex(jobs, j);

		edges.push_back({
			w_index,
			j_index,
			value
		});
	}

	OptimizerResult Optimize(OptimizerMode mode = OptimizerMode::MINIMIZE) {
		std::map<int, std::vector<Edge>> sorted_edges;
		// insert all edges
		for (Edge& e : edges)
			sorted_edges[e.worker_index].push_back(e);
		// sort every internal vector
		for (auto& kv : sorted_edges) {
			std::sort(kv.second.begin(), kv.second.end(), [mode](const Edge& a, const Edge& b) {
				return mode == OptimizerMode::MAXIMIZE ? (a.value > b.value) : (a.value < b.value);
			});
		}

		struct Solution {
			// index: worker int: points to the i-th Edge inside sorted_edges and NOT the jobs vector
			std::vector<int> assignments;
			// index: job int: worker
			std::vector<int> jobs_taken;

			int total_value;
			bool broken;
		};

		Solution best = {};
		best.assignments.resize(workers.size());
		best.jobs_taken.resize(jobs.size());
		best.total_value = 0;
		best.broken = true;
		std::vector<int> order;
		bool better;

		for (int i = 0; i < workers.size(); i++) {
			order.push_back(i);
			best.assignments[i] = -1;
		}
		for (int i = 0; i < jobs.size(); i++) {
			best.jobs_taken[i] = -1;
		}

		do {
			better = false;

			std::random_shuffle(order.begin(), order.end());

			for (int worker_index : order) {
				// get the current assignment index inside sorted_edges vector
				// or the max in the case no job has been picked yet
				int current_best_index = best.assignments[worker_index];
				if (current_best_index == -1) {
					current_best_index = sorted_edges[worker_index].size() - 1;
				}

				// any of the indices before will be better
				for (int i = 0; i <= current_best_index; i++) {
					Solution possible;
					possible.assignments = best.assignments;
					possible.jobs_taken = best.jobs_taken;
					possible.total_value = best.total_value;
					possible.broken = false;

					Edge& old_edge = sorted_edges[worker_index][current_best_index];
					Edge& new_edge = sorted_edges[worker_index][i];
					int replacing_worker = possible.jobs_taken[new_edge.job_index];

					possible.jobs_taken[new_edge.job_index] = worker_index;
					possible.jobs_taken[old_edge.job_index] = -1;
					possible.assignments[worker_index] = i;

					if (replacing_worker != -1) {
						for (int j = 0; j < sorted_edges[replacing_worker].size(); j++) {
							Edge& re = sorted_edges[replacing_worker][j];
							if (possible.jobs_taken[re.job_index] == -1) {
								possible.assignments[replacing_worker] = j;
								possible.jobs_taken[re.job_index] = replacing_worker;
								break;
							}
						}
					}

					for (int a : possible.assignments) {
						if (a == -1) {
							possible.broken = true;
							break;
						}
					}

					if (possible.broken <= best.broken) {
						possible.total_value = 0;
						for (int w_i = 0; w_i < workers.size(); w_i++) {
							int j_i = possible.assignments[w_i];
							if(j_i != -1)
								possible.total_value += sorted_edges[w_i][j_i].value;
						}
						bool improved = mode == OptimizerMode::MAXIMIZE ? (possible.total_value > best.total_value) : (possible.total_value < best.total_value);

						if (improved || possible.broken < best.broken) {
							//out::Log("--- Found better total: " + std::to_string(possible.total_value) + (possible.broken ? " (BROKEN)" : ""));
							best = possible;
							better = true;
							break;
						}
					}
				}
			}
		} while (better);

		OptimizerResult result;
		for (int worker_index = 0; worker_index < best.assignments.size(); worker_index++) {
			result.assignments[workers[worker_index]] = jobs[sorted_edges[worker_index][best.assignments[worker_index]].job_index];
		}
		result.total_value = best.total_value;

		return result;
	}
	
	OptimizerResult OptimizeOptimal(OptimizerMode mode = OptimizerMode::MINIMIZE) {
		if (workers.size() == 0) return {};

		if (jobs.size() > 750) {
			Reduce(750 / (int)(workers.size()));
		}

		int size = std::max(workers.size(), jobs.size());
		std::vector< std::vector<long long int> > matrix(size, std::vector<long long int>(size, mode == OptimizerMode::MINIMIZE ? BIG_INF : 0));

		for (const Edge& e : edges) {
			matrix[e.worker_index][e.job_index] = mode == OptimizerMode::MINIMIZE ? e.value : (BIG_INF - e.value);
		}

		/*
		out::Log("----------------------------------");
		out::Log("Workers: " + std::to_string(workers.size()) + " Jobs: " + std::to_string(jobs.size()));
		out::Log("Optimizing matrix: (size=" + std::to_string(size) + ")");
		
		for (int i = 0; i < matrix.size(); i++) {
			std::string s = "";
			for (int j = 0; j < matrix[i].size(); j++) {
				int val = mode == OptimizerMode::MINIMIZE ? matrix[i][j] : (BIG_INF - matrix[i][j]);
				s += (val == BIG_INF ? "X" : std::to_string(val)) + " ";
			}
			out::Log(s);
		}
		out::Log("----------------------------------");
		*/
		
		std::vector<int> flow_result = MinCostFlow(matrix);

		OptimizerResult result = {};
		for (int worker_index = 0; worker_index < workers.size(); worker_index++) {
			int job_index = flow_result[worker_index];
			result.assignments[workers[worker_index]] = jobs[job_index];
			result.total_value += mode == OptimizerMode::MINIMIZE ? matrix[worker_index][job_index] : (BIG_INF - matrix[worker_index][job_index]);
		}
		return result;
	}

private:
	struct Edge {
		int worker_index, job_index;
		long long int value;
	};
	std::vector<Worker> workers; // rows
	std::vector<Job> jobs; // columns
	std::vector<Edge> edges;

	template<typename T>
	int InsertAndGetIndex(std::vector<T>& v, T& t) {
		auto it = std::find(v.begin(), v.end(), t);
		if (it == v.end()) {
			v.push_back(t);
			return v.size() - 1;
		}
		else {
			return it - v.begin();
		}
	}

	/*
	Source: http://cp-algorithms.com/graph/Assignment-problem-min-flow.html
	O(N^5)
	*/
	std::vector<int> MinCostFlow(std::vector< std::vector<long long int> > a) {
		int n = a.size();
		int m = n * 2 + 2;
		std::vector<std::vector<int>> f(m, std::vector<int>(m));
		int s = m - 2, t = m - 1;
		long long int cost = 0;
		while (true) {
			std::vector<long long int> dist(m, BIG_INF);
			std::vector<int> p(m);
			std::vector<int> type(m, 2);
			std::deque<int> q;
			dist[s] = 0;
			p[s] = -1;
			type[s] = 1;
			q.push_back(s);
			while (!q.empty()) {
				int v = q.front();
				q.pop_front();
				type[v] = 0;
				if (v == s) {
					for (int i = 0; i < n; ++i) {
						if (f[s][i] == 0) {
							dist[i] = 0;
							p[i] = s;
							type[i] = 1;
							q.push_back(i);
						}
					}
				}
				else {
					if (v < n) {
						for (int j = n; j < n + n; ++j) {
							if (f[v][j] < 1 && dist[j] > dist[v] + a[v][j - n]) {
								dist[j] = dist[v] + a[v][j - n];
								p[j] = v;
								if (type[j] == 0)
									q.push_front(j);
								else if (type[j] == 2)
									q.push_back(j);
								type[j] = 1;
							}
						}
					}
					else {
						for (int j = 0; j < n; ++j) {
							if (f[v][j] < 0 && dist[j] > dist[v] - a[j][v - n]) {
								dist[j] = dist[v] - a[j][v - n];
								p[j] = v;
								if (type[j] == 0)
									q.push_front(j);
								else if (type[j] == 2)
									q.push_back(j);
								type[j] = 1;
							}
						}
					}
				}
			}

			long long int curcost = BIG_INF;
			for (int i = n; i < n + n; ++i) {
				if (f[i][t] == 0 && dist[i] < curcost) {
					curcost = dist[i];
					p[t] = i;
				}
			}
			if (curcost == BIG_INF)
				break;
			cost += curcost;
			for (int cur = t; cur != -1; cur = p[cur]) {
				int prev = p[cur];
				if (prev != -1)
					f[cur][prev] = -(f[prev][cur] = 1);
			}
		}

		std::vector<int> answer(n);
		for (int i = 0; i < n; ++i) {
			for (int j = 0; j < n; ++j) {
				if (f[i][j + n] == 1)
					answer[i] = j;
			}
		}
		return answer;
	}

	// Only use with OptimizeOptimal
	void Reduce(int reduce_factor) {
		std::vector<Job> new_jobs;
		std::vector<Edge> new_edges;
		std::vector<bool> already_in_new_jobs(jobs.size(), false);
		std::vector<int> counter(workers.size(), 0);

		std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
			return a.value > b.value;
		});

		while (new_jobs.size() < std::min(jobs.size(), reduce_factor * workers.size())) {
			int min_in_counter = INF;
			for (int k : counter)
				min_in_counter = std::min(k, min_in_counter);

			// find a good job to add
			// 'el primero que incremente una nave con el minimo'
			int job_to_add = -1;

			for (Edge& e : edges) {
				if (already_in_new_jobs[e.job_index]) continue; // if job already in new_jobs
				if (counter[e.worker_index] != min_in_counter) continue; // if its not a worker with the less jobs in new_jobs

				job_to_add = e.job_index;
				break;
			}
			assert(job_to_add == -1);

			already_in_new_jobs[job_to_add] = true;
			new_jobs.push_back(jobs[job_to_add]);
			int new_job_index = new_jobs.size() - 1;
			for (Edge& e : edges) {
				if (e.job_index == job_to_add) {
					counter[e.worker_index]++;
					new_edges.push_back({
						e.worker_index,
						new_job_index,
						e.value
					});
				}
			}
		}

		jobs = new_jobs;
		edges = new_edges;
	}
};