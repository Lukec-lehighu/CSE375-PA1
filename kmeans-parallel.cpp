// Implementation of the KMeans Algorithm
// reference: https://github.com/marcoscastro/kmeans

#include <iostream>
#include <fstream>
#include <vector>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <algorithm>
#include <chrono>

#include "oneapi/tbb.h"

using namespace std;
using namespace oneapi::tbb;

class Point
{
private:
	int id_point, id_cluster;
	vector<double> values;
	int total_values;
	string name;

public:
	Point(int id_point, vector<double>& values, string name = "")
	{
		this->id_point = id_point;
		total_values = values.size();

		for(int i = 0; i < total_values; i++)
			this->values.push_back(values[i]);

		this->name = name;
		id_cluster = -1;
	}

	int getID()
	{
		return id_point;
	}

	void setCluster(int id_cluster)
	{
		this->id_cluster = id_cluster;
	}

	int getCluster()
	{
		return id_cluster;
	}

	double getValue(int index)
	{
		return values[index];
	}

	int getTotalValues()
	{
		return total_values;
	}

	void addValue(double value)
	{
		values.push_back(value);
	}

	string getName()
	{
		return name;
	}
};

class Cluster
{
private:
	int id_cluster;
	vector<double> central_values;
	vector<Point> points;

public:
	Cluster(int id_cluster, Point point)
	{
		this->id_cluster = id_cluster;

		int total_values = point.getTotalValues();

		for(int i = 0; i < total_values; i++)
			central_values.push_back(point.getValue(i));

		points.push_back(point);
	}

	void addPoint(Point point)
	{
		points.push_back(point);
	}

	bool removePoint(int id_point)
	{
		int total_points = points.size();

		for(int i = 0; i < total_points; i++)
		{
			if(points[i].getID() == id_point)
			{
				points.erase(points.begin() + i);
				return true;
			}
		}
		return false;
	}

	double getCentralValue(int index)
	{
		return central_values[index];
	}

	void setCentralValue(int index, double value)
	{
		central_values[index] = value;
	}

	Point getPoint(int index)
	{
		return points[index];
	}

	int getTotalPoints()
	{
		return points.size();
	}

	int getID()
	{
		return id_cluster;
	}
};

class KMeans
{
private:
	int K; // number of clusters
	int total_values, total_points, max_iterations;
	vector<Cluster> clusters;

	// return ID of nearest center (uses euclidean distance)
	int getIDNearestCenter(Point point)
	{
		double sum = 0.0, min_dist;
		int id_cluster_center = 0;

		for(int i = 0; i < total_values; i++)
		{
			sum += pow(clusters[0].getCentralValue(i) -
					   point.getValue(i), 2.0);
		}

		min_dist = sqrt(sum);

		for(int i = 1; i < K; i++)
		{
			double dist;
			sum = 0.0;

			for(int j = 0; j < total_values; j++)
			{
				sum += pow(clusters[i].getCentralValue(j) -
						   point.getValue(j), 2.0);
			}

			dist = sqrt(sum);

			if(dist < min_dist)
			{
				min_dist = dist;
				id_cluster_center = i;
			}
		}

		return id_cluster_center;
	}

public:
	KMeans(int K, int total_points, int total_values, int max_iterations)
	{
		this->K = K;
		this->total_points = total_points;
		this->total_values = total_values;
		this->max_iterations = max_iterations;
	}

	void run(vector<Point> & points)
	{
        auto begin = chrono::high_resolution_clock::now();

		if(K > total_points)
			return;

		vector<int> prohibited_indexes;

		// choose K distinct values for the centers of the clusters
		for(int i = 0; i < K; i++)
		{
			// randomly select a point that isn't already selected
			while(true)
			{
				int index_point = rand() % total_points;

				if(find(prohibited_indexes.begin(), prohibited_indexes.end(),
						index_point) == prohibited_indexes.end())
				{
					prohibited_indexes.push_back(index_point);
					points[index_point].setCluster(i);
					Cluster cluster(i, points[index_point]);
					clusters.push_back(cluster);
					break;
				}
			}
		}
        auto end_phase1 = chrono::high_resolution_clock::now();

		int iter = 1;

		while(true)
		{
			bool done = true;

			// parallel for this, put points that need to be adjusted into a concurrent vector (tbb) and perform those operations after
			// other idea is to put a "dirty" bit in the points themselves and mark them as dirty when they need to be added to a different cluster

			// stores the index of the point and the id of the new cluster
			concurrent_vector<pair<int, int>> bad_points;

			// associates each point to the nearest center
			parallel_for(blocked_range<int>(1,total_points,80), [&](const blocked_range<int>& r) {
				for(int i=r.begin(); i<r.end(); i++) {
					int id_old_cluster = points[i].getCluster();
					int id_nearest_center = getIDNearestCenter(points[i]);

					if(id_old_cluster != id_nearest_center)
					{
						bad_points.push_back({i, id_nearest_center});
						done = false;
					}
				}
			});

			// after concurrent part: update the clusters with the points that were found to need updating
			for(auto &p : bad_points) {
				int id_old_cluster = points[p.first].getCluster();
				if(id_old_cluster != -1)
					clusters[id_old_cluster].removePoint(points[p.first].getID());

				points[p.first].setCluster(p.second);
				clusters[p.second].addPoint(points[p.first]);
			}

			// recalculating the center of each cluster
			for(int i = 0; i < K; i++)
			{
				for(int j = 0; j < total_values; j++)
				{
					int total_points_cluster = clusters[i].getTotalPoints();
					double sum = 0.0;

					if(total_points_cluster > 0)
					{
						sum = parallel_reduce(
							blocked_range<int>(0, total_points_cluster),
							double(0),
							[&](tbb::blocked_range<int>& r, double in) {
								for(int p = r.begin(); p < r.end(); p++)
									in += clusters[i].getPoint(p).getValue(j);
								return in;
							},
							std::plus<double>()
						);
						
						clusters[i].setCentralValue(j, sum / total_points_cluster);
					}
				}
			}

			if(done == true || iter >= max_iterations)
			{
				cout << "Break in iteration " << iter << "\n\n";
				break;
			}

			iter++;
		}
        auto end = chrono::high_resolution_clock::now();

		// shows elements of clusters
		for(int i = 0; i < K; i++)
		{
			int total_points_cluster =  clusters[i].getTotalPoints();

			cout << "Cluster " << clusters[i].getID() + 1 << endl;
			for(int j = 0; j < total_points_cluster; j++)
			{
				cout << "Point " << clusters[i].getPoint(j).getID() + 1 << ": ";
				for(int p = 0; p < total_values; p++)
					cout << clusters[i].getPoint(j).getValue(p) << " ";

				string point_name = clusters[i].getPoint(j).getName();

				if(point_name != "")
					cout << "- " << point_name;

				cout << endl;
			}

			cout << "Cluster values: ";

			for(int j = 0; j < total_values; j++)
				cout << clusters[i].getCentralValue(j) << " ";

			cout << "\n\n";
            cout << "TOTAL EXECUTION TIME = "<<std::chrono::duration_cast<std::chrono::microseconds>(end-begin).count()<<"\n";

            cout << "TIME PHASE 1 = "<<std::chrono::duration_cast<std::chrono::microseconds>(end_phase1-begin).count()<<"\n";

            cout << "TIME PHASE 2 = "<<std::chrono::duration_cast<std::chrono::microseconds>(end-end_phase1).count()<<"\n";
		}
	}
};

vector<double> splitAndConvert(string& in, int expected, char delim) {
	vector<double> res;
	while(in.size() > 0) {
		string tmp = in.substr(0, in.find(delim));

		try {
			res.push_back(stod(tmp));
		} catch(...) {
			return res;
		}

		int next = in.find(delim);
		if(next == string::npos) break;
		in = in.substr(next+1);

		if(res.size() >= expected) break;
	}
	return res;
}

int main(int argc, char *argv[])
{
	srand (time(NULL));

	char delim = ' ';
	if(argc < 2) {
		cout << "Usage: ./kmeans <filename>" << endl;
		return 0;
	}
	if(argc >= 3)
		delim = argv[2][0];

	ifstream dataFile(argv[1]);

	int total_points, total_values, K, max_iterations, has_name;
	string line;
	if(getline(dataFile, line)) {
		auto nums = splitAndConvert(line, 5, ' ');
		if(nums.size() != 5) {
			cout << "Improperly formatted header line!" << endl;
			return 0;
		}

		total_points = nums[0];
		total_values = nums[1];
		K = nums[2];
		max_iterations = nums[3];
		has_name = nums[4];
	} else {
		cout << "Input file missing line" << endl;
		dataFile.close();
		return 0;
	}

	vector<Point> points;
	string point_name;

	for(int i = 0; i < total_points; i++)
	{
		vector<double> values;
		if(!getline(dataFile, line)) {
			cout << "Improper number of lines" << endl;
			break;
		}

		vector<double> fileValues = splitAndConvert(line, total_values, delim);
		if(fileValues.size() != total_values) continue;

		for(int j = 0; j < total_values; j++)
		{
			values.push_back(fileValues[j]);
		}

		if(has_name)
		{
			Point p(i, values, line);
			points.push_back(p);
		}
		else
		{
			Point p(i, values);
			points.push_back(p);
		}
	}
	dataFile.close();

	KMeans kmeans(K, points.size(), total_values, max_iterations);
	kmeans.run(points);

	return 0;
}
