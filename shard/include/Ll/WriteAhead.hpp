
namespace Ll {

class WriteAhead {
public:
	typedef int64_t sequence_type;
	
	WriteAhead();

	void setPath(const std::string &path);
	void setIdentifier(const std::string &identifier);
	
	void createLog();
	void loadLog();
	
	void submit(Db::Proto::WriteAhead &message,
			std::function<void()> callback);

private:
	std::string p_path;
	std::string p_identifier;
	
	std::unique_ptr<Linux::File> p_file;
	
	sequence_type p_curSequence;
};

}

