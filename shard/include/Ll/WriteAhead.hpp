
namespace Ll {

class WriteAhead {
public:
	WriteAhead();

	void setPath(const std::string &path);
	void setIdentifier(const std::string &identifier);
	
	void createLog();
	void loadLog();
	
	void log(Db::Proto::LogEntry &message,
			std::function<void(Error)> callback);

private:
	std::string p_path;
	std::string p_identifier;
	
	std::unique_ptr<Linux::File> p_file;
};

}

