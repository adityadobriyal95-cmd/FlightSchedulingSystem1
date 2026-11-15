#pragma once
#include "Database.hpp"
#include "Utils.hpp"
#include "httplib.h"

class ApiRoutes {
public:
    static void registerAll(httplib::Server& server, Database& db, SessionManager& sessions);
};
