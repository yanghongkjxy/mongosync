#ifndef MONGO_SYNC_H
#define MONGO_SYNC_H
#include <string>

#include "mongo/client/dbclient.h"

#define BATCH_BUFFER_SIZE (16*1024*1024)


enum OplogProcessOp {
	kClone,
	kApply
};

struct OplogTime {
	bool empty() { //mark whether to be initialized
		return sec == -1 && no == -1;
	}

	mongo::Timestamp_t timestamp() {
		return mongo::Timestamp_t(sec, no);
	}

	OplogTime(const mongo::Timestamp_t& t) {
		sec = t.seconds();
		no = t.increment();
	}

	const OplogTime& operator=(const mongo::Timestamp_t& t) {
		sec = t.seconds();
		no = t.increment();
	}

	OplogTime(int32_t _sec = -1, int32_t _no = -1)
		: sec(_sec),
		no(_no) {
	}

	int32_t no; //the logical time rank with 1 second
	int32_t sec; //the unix time in secon
};

struct Options {
	Options() : oplog(false), raw_oplog(false), no_index(false), dst_oplog_ns("sync.oplog") {}

	std::string src_ip_port;
	std::string src_user;
	std::string src_passwd;
	std::string src_auth_db;

	std::string dst_ip_port;
	std::string dst_user;
	std::string dst_passwd;
	std::string dst_auth_db;

//the database or collection to be transfered	
	std::string db;
	std::string dst_db;
	std::string coll;
	std::string dst_coll;

	bool oplog;
	OplogTime oplog_start;
	OplogTime oplog_end;  //the time is inclusive

	bool raw_oplog;
	std::string dst_oplog_ns;

	bool no_index;
	mongo::Query filter;
};

class NamespaceString {
public:
	NamespaceString()
		: dot_index_(std::string::npos) {
	}

	NamespaceString(std::string ns)
		: ns_(ns) {
		dot_index_ = ns_.find_first_of(".");
	}

	NamespaceString(std::string db, std::string coll) {
		ns_ = db + "." + coll;
		dot_index_ = db.size();
	}
	
	std::string db() {
		if (dot_index_ == std::string::npos) {
			return std::string();
		}
		return ns_.substr(0, dot_index_);
	}

	std::string coll() {
		if (dot_index_ == std::string::npos || dot_index_ + 1 >= ns_.size()) {
			return std::string();
		}
		return ns_.substr(dot_index_+1);
	}
	
	std::string ns() {
		return ns_;
	}
private:
		std::string ns_;
		int32_t dot_index_;
};

void ParseOptions(int argc, char** argv, Options* opt);

class MongoSync {
public:
	static MongoSync* NewMongoSync(const Options& opt);
	MongoSync(const Options& opt);
	~MongoSync();
	int32_t InitConn();

	void Process();
	void CloneOplog();
	void CloneDb();
	void CloneColl(std::string src_ns, std::string dst_ns, int32_t batch_size = BATCH_BUFFER_SIZE);
	void SyncOplog();

private:
	Options opt_;
	mongo::DBClientConnection* src_conn_;
	mongo::DBClientConnection* dst_conn_;
	std::string src_version_;
	std::string dst_version_;

	OplogTime oplog_begin_;
	OplogTime oplog_finish_;

	const std::string oplog_ns_ = "local.oplog.rs"; // TODO: Is it const

	void CloneCollIndex(std::string sns, std::string dns);
	void GenericProcessOplog(OplogProcessOp op);
	void ProcessSingleOplog(const std::string& db, const std::string& coll, std::string& dst_db, std::string& dst_coll, const mongo::BSONObj& oplog, OplogProcessOp op);
	void ApplyInsertOplog(const std::string& dst_db, const std::string& dst_coll, const mongo::BSONObj& oplog);
	void ApplyCmdOplog(const std::string& dst_db, const std::string& dst_coll, const mongo::BSONObj& oplog, bool same_coll = true);
	mongo::DBClientConnection* ConnectAndAuth(std::string srv_ip_port, std::string auth_db, std::string user, std::string passwd);
	OplogTime GetSideOplogTime(mongo::DBClientConnection* conn, std::string ns, std::string db, std::string coll, bool first_or_last); //first_or_last==true->get the first timestamp; first_or_last==false->get the last timestamp

	std::string GetMongoVersion(mongo::DBClientConnection* conn);
	int GetCollIndexesByVersion(mongo::DBClientConnection* conn, std::string version, std::string ns, mongo::BSONObj& indexes);
	void SetCollIndexesByVersion(mongo::DBClientConnection* conn, std::string version, std::string coll_full_name, mongo::BSONObj index);
	int GetAllCollByVersion(mongo::DBClientConnection* conn, std::string version, std::string db, std::vector<std::string>& colls);
	


	bool need_clone_oplog() {
		return opt_.raw_oplog;
	}

	bool need_clone_db() {
		return !opt_.raw_oplog && !opt_.db.empty() && opt_.coll.empty() && opt_.oplog_start.empty() && opt_.oplog_end.empty();
		/*opt_.raw_oplog && (!opt_.db.empty() && opt_.coll.empty() && opt_.oplog_start.empty() && opt_.oplog_end.empty()
		 * || opt_.db.empty() && opt_.coll.empty() && opt_.oplog_start.empty() && opt_.oplog_end.empty());
		 * */
	}

	bool need_clone_coll() {
		return !opt_.raw_oplog && !opt_.coll.empty() && opt_.oplog_start.empty() && opt_.oplog_end.empty();
	}

	bool need_sync_oplog() {
		return !opt_.raw_oplog && opt_.oplog;
	}

	MongoSync(const MongoSync&);
	MongoSync& operator=(const MongoSync&);
};
#endif
