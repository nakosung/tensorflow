#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/public/session.h"

__declspec(dllexport) void hello_tensorflow(const char* fname) {
	using namespace tensorflow;

	GraphDef graph_def;

	auto load_graph_status = ReadBinaryProto(Env::Default(), std::string(fname), &graph_def);

	{
		std::vector<Tensor> output;
		std::vector<string> vNames;

		int node_count = graph_def.node_size();
		for (int i = 0; i < node_count; i++) {
			auto n = graph_def.node(i);

			if (n.name().find("nWeights") != std::string::npos) {
				vNames.push_back(n.name());
			}
		}
	}

	SessionOptions options;
	std::unique_ptr<Session> session(NewSession(options));

	session->Create(graph_def);

	std::vector<std::pair<std::string, Tensor>> input;

	Tensor inputTensor(DT_FLOAT, TensorShape({ 1,10 }));
	
	input.emplace_back(std::string("param"), inputTensor);
	std::vector<Tensor> outputs;

	outputs.clear();
	TF_CHECK_OK(session->Run(input, { "output" }, {}, &outputs));

	session->Close();
}