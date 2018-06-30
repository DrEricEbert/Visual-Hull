#pragma warning(disable:4819)
#pragma warning(disable:4244)
#pragma warning(disable:4267)

#include <time.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <opencv2/opencv.hpp>
#include <Eigen/Eigen>
#include <limits>
#include "octree.cpp"
// �����ж�ͶӰ�Ƿ���visual hull�ڲ�
struct Projection
{
	Eigen::Matrix<float, 3, 4> m_projMat;
	cv::Mat m_image;
	const uint m_threshold = 125;

	bool outOfRange(int x, int max)
	{
		return x < 0 || x >= max;
	}

	bool checkRange(double x, double y, double z)
	{
		Eigen::Vector3f vec3 = m_projMat * Eigen::Vector4f(x, y, z, 1);
		int indX = vec3[1] / vec3[2];
		int indY = vec3[0] / vec3[2];

		if (outOfRange(indX, m_image.size().height) || outOfRange(indY, m_image.size().width))
			return false;
		return m_image.at<uchar>((uint)(vec3[1] / vec3[2]), (uint)(vec3[0] / vec3[2])) > m_threshold;
	}
};

// ����index��ʵ������֮���ת��
struct CoordinateInfo
{
	int m_resolution;
	double m_min;
	double m_max;

	double index2coor(int index)
	{
		return m_min + index * (m_max - m_min) / m_resolution;
	}
	
	CoordinateInfo(int resolution = 10, double min = 0.0, double max = 100.0)
		: m_resolution(resolution)
		, m_min(min)
		, m_max(max)
	{
	}
};

class Model
{
public:
	typedef std::vector<std::vector<bool>> Pixel;
	typedef std::vector<Pixel> Voxel;
	typedef std::vector<Eigen::Vector3i> PointSequenceInd;//���±��ʾ�Ķ�������
	
	Model(int resX = 100, int resY = 100, int resZ = 100);
	~Model();

	void saveModel(const char* pFileName);
	void saveModelWithNormal(const char* pFileName);
	void loadMatrix(const char* pFileName);
	void loadImage(const char* pDir, const char* pPrefix, const char* pSuffix);
	void getModel();
	void getSurface();
	void getModelUsingOctree();
	
	void updateNode(Node * node);
	void updateTree(Node *node);
	Eigen::Vector3f getNormal(int indX, int indY, int indZ);
	Status searchPoint(int xind, int yind, int zind, Node* node);
private:
	CoordinateInfo m_corrX;
	CoordinateInfo m_corrY;
	CoordinateInfo m_corrZ;
	octree* Octree;
	int m_neiborSize;
	Eigen::Vector3f indexVecToCorrVec(Eigen::Vector3i& indexVec);
	std::vector<Projection> m_projectionList;

	Voxel m_voxel;
	Voxel m_surface;
};
Eigen::Vector3f Model::indexVecToCorrVec(Eigen::Vector3i& indexVec) {
	Eigen::Vector3f temp;
	temp[0] = m_corrX.index2coor(indexVec[0]);
	temp[1] = m_corrY.index2coor(indexVec[1]);
	temp[2] = m_corrZ.index2coor(indexVec[2]);
	return temp;
}
Model::Model(int resX, int resY, int resZ)
	: m_corrX(resX, -5, 5)
	, m_corrY(resY, -10, 10)
	, m_corrZ(resZ, 15, 30)
{
	if (resX > 100)
		m_neiborSize = resX / 100;
	else
		m_neiborSize = 1;
	m_voxel = Voxel(m_corrX.m_resolution, Pixel(m_corrY.m_resolution, std::vector<bool>(m_corrZ.m_resolution, true)));
	m_surface = m_voxel;
	Octree = new octree(resX-1, resY-1, resZ-1);
}

Model::~Model()
{
}

void Model::saveModel(const char* pFileName)
{
	std::ofstream fout(pFileName);

	for (int indexX = 0; indexX < m_corrX.m_resolution; indexX++)
		for (int indexY = 0; indexY < m_corrY.m_resolution; indexY++)
			for (int indexZ = 0; indexZ < m_corrZ.m_resolution; indexZ++)
				if (m_surface[indexX][indexY][indexZ])
				{
					double coorX = m_corrX.index2coor(indexX);
					double coorY = m_corrY.index2coor(indexY);
					double coorZ = m_corrZ.index2coor(indexZ);
					fout << coorX << ' ' << coorY << ' ' << coorZ << std::endl;
				}
}

void Model::saveModelWithNormal(const char* pFileName)
{
	std::ofstream fout(pFileName);

	double midX = m_corrX.index2coor(m_corrX.m_resolution / 2);
	double midY = m_corrY.index2coor(m_corrY.m_resolution / 2);
	double midZ = m_corrZ.index2coor(m_corrZ.m_resolution / 2);

	for (int indexX = 0; indexX < m_corrX.m_resolution; indexX++)
		for (int indexY = 0; indexY < m_corrY.m_resolution; indexY++)
			for (int indexZ = 0; indexZ < m_corrZ.m_resolution; indexZ++)
				if (m_surface[indexX][indexY][indexZ])
				{
					double coorX = m_corrX.index2coor(indexX);
					double coorY = m_corrY.index2coor(indexY);
					double coorZ = m_corrZ.index2coor(indexZ);
					fout << coorX << ' ' << coorY << ' ' << coorZ << ' ';

					Eigen::Vector3f nor = getNormal(indexX, indexY, indexZ);
					fout << nor(0) << ' ' << nor(1) << ' ' << nor(2) << std::endl;
				}
}

void Model::loadMatrix(const char* pFileName)
{
	std::ifstream fin(pFileName);

	int num;
	Eigen::Matrix<float, 3, 3> matInt;
	Eigen::Matrix<float, 3, 4> matExt;
	Projection projection;
	while (fin >> num)
	{
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++)
				fin >> matInt(i, j);

		double temp;
		fin >> temp;
		fin >> temp;
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 4; j++)
				fin >> matExt(i, j);

		projection.m_projMat = matInt * matExt;
		m_projectionList.push_back(projection);
	}
}

void Model::loadImage(const char* pDir, const char* pPrefix, const char* pSuffix)
{
	int fileCount = m_projectionList.size();
	std::string fileName(pDir);
	fileName += '/';
	fileName += pPrefix;
	for (int i = 0; i < fileCount; i++)
	{
		std::cout << fileName + std::to_string(i) + pSuffix << std::endl;
		m_projectionList[i].m_image = cv::imread(fileName + std::to_string(i) + pSuffix, CV_8UC1);
	}
}

void Model::getModel()
{
	int prejectionCount = m_projectionList.size();

	for (int indexX = 0; indexX < m_corrX.m_resolution; indexX++)
		for (int indexY = 0; indexY < m_corrY.m_resolution; indexY++)
			for (int indexZ = 0; indexZ < m_corrZ.m_resolution; indexZ++)
				for (int i = 0; i < prejectionCount; i++)
				{
					double coorX = m_corrX.index2coor(indexX);
					double coorY = m_corrY.index2coor(indexY);
					double coorZ = m_corrZ.index2coor(indexZ);
					m_voxel[indexX][indexY][indexZ] = m_voxel[indexX][indexY][indexZ] && m_projectionList[i].checkRange(coorX, coorY, coorZ);
				}
}

void Model::getSurface()
{
	// �����ϡ��¡����ҡ�ǰ����
	int dx[6] = { -1, 0, 0, 0, 0, 1 };
	int dy[6] = { 0, 1, -1, 0, 0, 0 };
	int dz[6] = { 0, 0, 0, 1, -1, 0 };

	// lambda���ʽ�������ж�ĳ�����Ƿ���Voxel�ķ�Χ��
	auto outOfRange = [&](int indexX, int indexY, int indexZ) {
		return indexX < 0 || indexY < 0 || indexZ < 0
			|| indexX >= m_corrX.m_resolution
			|| indexY >= m_corrY.m_resolution
			|| indexZ >= m_corrZ.m_resolution;
	};

	for (int indexX = 0; indexX < m_corrX.m_resolution; indexX++)
		for (int indexY = 0; indexY < m_corrY.m_resolution; indexY++)
			for (int indexZ = 0; indexZ < m_corrZ.m_resolution; indexZ++)
			{
				if (!m_voxel[indexX][indexY][indexZ])
				{
					m_surface[indexX][indexY][indexZ] = false;
					continue;
				}
				bool ans = false;
				for (int i = 0; i < 6; i++)
				{
					ans = ans || outOfRange(indexX + dx[i], indexY + dy[i], indexZ + dz[i])
						|| !m_voxel[indexX + dx[i]][indexY + dy[i]][indexZ + dz[i]];
				}
				m_surface[indexX][indexY][indexZ] = ans;
			}
}

Eigen::Vector3f Model::getNormal(int indX, int indY, int indZ)
{
	auto outOfRange = [&](int indexX, int indexY, int indexZ) {
		return indexX < 0 || indexY < 0 || indexZ < 0
			|| indexX >= m_corrX.m_resolution
			|| indexY >= m_corrY.m_resolution
			|| indexZ >= m_corrZ.m_resolution;
	};

	std::vector<Eigen::Vector3f> neiborList;
	std::vector<Eigen::Vector3f> innerList;

	for (int dX = -m_neiborSize; dX <= m_neiborSize; dX++)
		for (int dY = -m_neiborSize; dY <= m_neiborSize; dY++)
			for (int dZ = -m_neiborSize; dZ <= m_neiborSize; dZ++)
			{
				if (!dX && !dY && !dZ)
					continue;
				int neiborX = indX + dX;
				int neiborY = indY + dY;
				int neiborZ = indZ + dZ;
				if (!outOfRange(neiborX, neiborY, neiborZ))
				{
					float coorX = m_corrX.index2coor(neiborX);
					float coorY = m_corrY.index2coor(neiborY);
					float coorZ = m_corrZ.index2coor(neiborZ);
					if (m_surface[neiborX][neiborY][neiborZ])
						neiborList.push_back(Eigen::Vector3f(coorX, coorY, coorZ));
					else if (m_voxel[neiborX][neiborY][neiborZ])
						innerList.push_back(Eigen::Vector3f(coorX, coorY, coorZ));
				}
			}

	Eigen::Vector3f point(m_corrX.index2coor(indX), m_corrY.index2coor(indY), m_corrZ.index2coor(indZ));

	Eigen::MatrixXf matA(3, neiborList.size());
	for (int i = 0; i < neiborList.size(); i++)
		matA.col(i) = neiborList[i] - point;
	Eigen::SelfAdjointEigenSolver<Eigen::Matrix3f> eigenSolver(matA * matA.transpose());
	Eigen::Vector3f eigenValues = eigenSolver.eigenvalues();
	int indexEigen = 0;
	if (abs(eigenValues[1]) < abs(eigenValues[indexEigen]))
		indexEigen = 1;
	if (abs(eigenValues[2]) < abs(eigenValues[indexEigen]))
		indexEigen = 2;
	Eigen::Vector3f normalVector = eigenSolver.eigenvectors().col(indexEigen);

	Eigen::Vector3f innerCenter = Eigen::Vector3f::Zero();
	for (auto const& vec : innerList)
		innerCenter += vec;
	innerCenter /= innerList.size();

	if (normalVector.dot(point - innerCenter) < 0)
		normalVector *= -1;
	return normalVector;
}
void Model::updateNode(Node* node) {
	PointSequenceInd pointsInd(8);
	
	Eigen::Vector3i temp;
	for (int i = 0; i < 8; i++) {
		if (i < 4) 
			pointsInd[i][0] = node->xmin;
		else pointsInd[i][0] = node->xmax;
		if (i == 0 || i == 1 || i == 4 || i == 5) pointsInd[i][1] = node->ymin;
		else pointsInd[i][1] = node->ymax;
		if (i == 0 || i == 2 || i == 4 || i == 6) pointsInd[i][2] = node->zmin;
		else pointsInd[i][2] = node->zmax;

	}
	bool allIn = true;
	for (int j = 0; j < m_projectionList.size(); j++) {
		bool allOut = false;
		
		for (int i = 0; i < 8; i++)
		{
			Eigen::Vector3f vec = indexVecToCorrVec(pointsInd[i]);
			bool temp = m_projectionList[j].checkRange(vec[0], vec[1], vec[2]);
			allOut = allOut||temp;
			allIn = allIn&&temp;
		}
		if (!allOut) { node->status = Status::OUT; return; }
		
	}
	if (allIn) {
		node->status = Status::IN;
		return;
	}
	else {
		node->status = Status::ON;
		return;
	}
}
Status Model::searchPoint(int x, int y, int z, Node* p) {
	if (p->children.size() == 0) return  p->status;
	else {
		if (x >= p->xmin&&x < p->getXmid())
		{
			if (y >= p->ymin&&y < p->getYmid()) {
				if (z >= p->zmin&&z < p->getZmid()) {
					return searchPoint(x, y, z, p->children[0]);
				}
				else {
					return searchPoint(x, y, z, p->children[1]);
				}
			}
			else {
				if (z >= p->zmin&&z < p->getZmid()) {
					return searchPoint(x, y, z, p->children[2]);
				}
				else {
					return searchPoint(x, y, z, p->children[3]);
				}
			}
		}
		else {
			if (y >= p->ymin&&y < p->getYmid()) {
				if (z >= p->zmin&&z < p->getZmid()) {
					return searchPoint(x, y, z, p->children[4]);
				}
				else {
					return searchPoint(x, y, z, p->children[5]);
				}
			}
			else {
				if (z >= p->zmin&&z < p->getZmid()) {
					return searchPoint(x, y, z, p->children[6]);
				}
				else {
					return searchPoint(x, y, z, p->children[7]);
				}
			}
		}
	}
}
void Model::getModelUsingOctree() {
	Octree->rootNode->status = Status::ON;
	updateTree(Octree->rootNode);
	for (int indexX = 0; indexX < m_corrX.m_resolution; indexX++)
		for (int indexY = 0; indexY < m_corrY.m_resolution; indexY++)
			for (int indexZ = 0; indexZ < m_corrZ.m_resolution; indexZ++) {
				Status p = searchPoint(indexX, indexY, indexZ, Octree->rootNode);
				if ( p== Status::OUT) {
					m_voxel[indexX][indexY][indexZ] = 0;
					m_surface[indexX][indexY][indexZ] = 0;
				}
				else {
					if (p == Status::IN) {
						m_surface[indexX][indexY][indexZ] = 0;
					}
				}
			
			}
}

void Model::updateTree(Node* node) {
	if (node->status == Status::NONE) {
		updateNode(node);
	}
		switch (node->status) {
		case Status::IN:
		case Status::OUT:
			return;
		case Status::ON:
		{
			int childnum=node->split();
			if (!childnum) return;
			else {
				for (int k = 0; k < 8; k++)
					updateTree(node->children[k]);
			}
		}
		}
	
}



int main(int argc, char** argv)
{
	clock_t t = clock();

	// �ֱ�����xyz�����Voxel�ֱ���
	Model model(300, 300, 300);

	// ��ȡ������������
	model.loadMatrix("../../calibParamsI.txt");

	// ��ȡͶӰͼƬ
	model.loadImage("../../wd_segmented", "WD2_", "_00020_segmented.png");

	// �õ�Voxelģ��
	model.getModel();
	std::cout << "get model done\n";

	// ���Voxelģ�͵ı���
	model.getSurface();
	/*model.getModelUsingOctree();*/
	std::cout << "get surface done\n";

	// ��ģ�͵���Ϊxyz��ʽ
	model.saveModel("../../WithoutNormal.xyz");
	std::cout << "save without normal done\n";

	model.saveModelWithNormal("../../WithNormal.xyz");
	std::cout << "save with normal done\n";

	system("PoissonRecon.x64 --in ../../WithNormal.xyz --out ../../mesh.ply");
	std::cout << "save mesh.ply done\n";

	t = clock() - t;
	std::cout << "time: " << (float(t) / CLOCKS_PER_SEC) << "seconds\n";
	system("pause");
	return (0);
}