/*
 * Cppcheck - A tool for static C/C++ code analysis
 * Copyright (C) 2007-2025 Cppcheck team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "checkersreport.h"

#include "addoninfo.h"
#include "checkers.h"
#include "errortypes.h"
#include "settings.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <sstream>
#include <unordered_set>
#include <vector>

static bool isCppcheckPremium(const Settings& settings) {
    return (settings.cppcheckCfgProductName.compare(0, 16, "Cppcheck Premium") == 0);
}

static int getMisraCVersion(const Settings& settings) {
    if (settings.premiumArgs.find("misra-c-2025") != std::string::npos)
        return 2025;
    if (settings.premiumArgs.find("misra-c-2023") != std::string::npos)
        return 2023;
    if (settings.premiumArgs.find("misra-c-2012") != std::string::npos)
        return 2012;
    if (settings.addons.count("misra"))
        return 2012;
    const bool misraAddonInfo = std::any_of(settings.addonInfos.cbegin(), settings.addonInfos.cend(), [](const AddonInfo& addonInfo) {
        return addonInfo.name == "misra";
    });
    if (misraAddonInfo)
        return 2012;
    return 0;
}

static bool isMisraRuleActive(const std::set<std::string>& activeCheckers, const std::string& rule) {
    if (activeCheckers.count("Misra C: " + rule))
        return true;
    if (rule == "1.1")
        return true; // syntax error
    if (rule == "1.3")
        return true; // undefined behavior
    if (rule == "2.1")
        return activeCheckers.count("CheckCondition::alwaysTrueFalse") != 0;
    if (rule == "2.6")
        return activeCheckers.count("CheckOther::checkUnusedLabel") != 0;
    if (rule == "2.8")
        return activeCheckers.count("CheckUnusedVar::checkFunctionVariableUsage") != 0;
    if (rule == "5.3")
        return activeCheckers.count("CheckOther::checkShadowVariables") != 0;
    if (rule == "8.13")
        return activeCheckers.count("CheckOther::checkConstPointer") != 0;
    if (rule == "9.1")
        return true; // uninitvar
    if (rule == "12.5")
        return activeCheckers.count("CheckOther::checkConstPointer") != 0;
    if (rule == "14.3")
        return activeCheckers.count("CheckCondition::alwaysTrueFalse") != 0;
    if (rule == "17.5")
        return activeCheckers.count("CheckBufferOverrun::argumentSize") != 0;
    if (rule == "18.1")
        return activeCheckers.count("CheckBufferOverrun::pointerArithmetic") != 0;
    if (rule == "18.2")
        return activeCheckers.count("CheckOther::checkComparePointers") != 0;
    if (rule == "18.3")
        return activeCheckers.count("CheckOther::checkComparePointers") != 0;
    if (rule == "18.6")
        return true; // danlingLifetime => error
    if (rule == "19.1")
        return activeCheckers.count("CheckOther::checkOverlappingWrite") != 0;
    if (rule == "20.6")
        return true; // preprocessorErrorDirective
    if (rule == "21.13")
        return activeCheckers.count("CheckFunctions::invalidFunctionUsage") != 0;
    if (rule == "21.17")
        return activeCheckers.count("CheckBufferOverrun::bufferOverflow") != 0;
    if (rule == "21.18")
        return activeCheckers.count("CheckBufferOverrun::bufferOverflow") != 0;
    if (rule == "22.1")
        return true;  // memleak => error
    if (rule == "22.2")
        return activeCheckers.count("CheckAutoVariables::autoVariables") != 0;
    if (rule == "22.3")
        return activeCheckers.count("CheckIO::checkFileUsage") != 0;
    if (rule == "22.4")
        return activeCheckers.count("CheckIO::checkFileUsage") != 0;
    if (rule == "22.6")
        return activeCheckers.count("CheckIO::checkFileUsage") != 0;

    return false;
}

CheckersReport::CheckersReport(const Settings& settings, const std::set<std::string>& activeCheckers)
    : mSettings(settings), mActiveCheckers(activeCheckers)
{}

int CheckersReport::getActiveCheckersCount()
{
    if (mAllCheckersCount == 0) {
        countCheckers();
    }
    return mActiveCheckersCount;
}

int CheckersReport::getAllCheckersCount()
{
    if (mAllCheckersCount == 0) {
        countCheckers();
    }
    return mAllCheckersCount;
}

void CheckersReport::countCheckers()
{
    mActiveCheckersCount = mAllCheckersCount = 0;

    for (const auto& checkReq: checkers::allCheckers) {
        if (mActiveCheckers.count(checkReq.first) > 0)
            ++mActiveCheckersCount;
        ++mAllCheckersCount;
    }
    for (const auto& checkReq: checkers::premiumCheckers) {
        if (mActiveCheckers.count(checkReq.first) > 0)
            ++mActiveCheckersCount;
        ++mAllCheckersCount;
    }
    if (mSettings.premiumArgs.find("misra-c-") != std::string::npos || mSettings.addons.count("misra")) {
        for (const checkers::MisraInfo& info: checkers::misraC2012Rules) {
            const std::string rule = std::to_string(info.a) + "." + std::to_string(info.b);
            const bool active = isMisraRuleActive(mActiveCheckers, rule);
            if (active)
                ++mActiveCheckersCount;
            ++mAllCheckersCount;
        }
    }
}

std::string CheckersReport::getReport(const std::string& criticalErrors) const
{
    std::ostringstream fout;

    fout << "Critical errors" << std::endl;
    fout << "---------------" << std::endl;
    if (!criticalErrors.empty()) {
        fout << "There were critical errors (" << criticalErrors << ")." << std::endl;
        fout << "These cause the analysis of the file to end prematurely." << std::endl;
    } else {
        fout << "No critical errors encountered." << std::endl;
        // TODO: mention "information" and "debug" as source for indications of bailouts
        // TODO: still rephrase this - this message does not provides confidence in the results
        // TODO: document what a bailout is and why it is done - mention it in the upcoming security/tuning guide
        // TODO: make bailouts a seperate group - need to differentiate between user bailouts (missing data like configuration/includes) and internal bailouts (e.g. limitations of ValueFlow)
        fout << "Note: There might still have been non-critical bailouts which might lead to false negatives." << std::endl;
    }

    fout << std::endl << std::endl;
    fout << "Open source checkers" << std::endl;
    fout << "--------------------" << std::endl;

    std::size_t maxCheckerSize = 0;
    for (const auto& checkReq: checkers::allCheckers) {
        const std::string& checker = checkReq.first;
        maxCheckerSize = std::max(checker.size(), maxCheckerSize);
    }
    for (const auto& checkReq: checkers::allCheckers) {
        const std::string& checker = checkReq.first;
        const bool active = mActiveCheckers.count(checkReq.first) > 0;
        const std::string& req = checkReq.second;
        fout << (active ? "Yes  " : "No   ") << checker;
        if (!active && !req.empty())
            fout << std::string(maxCheckerSize + 4 - checker.size(), ' ') << "require:" + req;
        fout << std::endl;
    }

    const bool cppcheckPremium = isCppcheckPremium(mSettings);

    auto reportSection = [&fout, cppcheckPremium]
                         (const std::string& title,
                          const Settings& settings,
                          const std::set<std::string>& activeCheckers,
                          const std::map<std::string, std::string>& premiumCheckers,
                          const std::string& substring) {
        fout << std::endl << std::endl;
        fout << title << std::endl;
        fout << std::string(title.size(), '-') << std::endl;
        if (!cppcheckPremium) {
            fout << "Not available, Cppcheck Premium is not used" << std::endl;
            return;
        }
        int maxCheckerSize = 0;
        for (const auto& checkReq: premiumCheckers) {
            const std::string& checker = checkReq.first;
            if (checker.find(substring) != std::string::npos && checker.size() > maxCheckerSize)
                maxCheckerSize = checker.size();
        }
        for (const auto& checkReq: premiumCheckers) {
            const std::string& checker = checkReq.first;
            if (checker.find(substring) == std::string::npos)
                continue;
            std::string req = checkReq.second;
            bool active = cppcheckPremium && activeCheckers.count(checker) > 0;
            if (substring == "::") {
                if (req == "warning")
                    active &= settings.severity.isEnabled(Severity::warning);
                else if (req == "style")
                    active &= settings.severity.isEnabled(Severity::style);
                else if (req == "portability")
                    active &= settings.severity.isEnabled(Severity::portability);
                else if (!req.empty())
                    active = false; // FIXME: handle req
            }
            fout << (active ? "Yes  " : "No   ") << checker;
            if (!cppcheckPremium) {
                if (!req.empty())
                    req = "premium," + req;
                else
                    req = "premium";
            }
            if (!req.empty())
                req = "require:" + req;
            if (!active)
                fout << std::string(maxCheckerSize + 4 - checker.size(), ' ') << req;
            fout << std::endl;
        }
    };

    reportSection("Premium checkers", mSettings, mActiveCheckers, checkers::premiumCheckers, "::");
    reportSection("Autosar", mSettings, mActiveCheckers, checkers::premiumCheckers, "Autosar: ");
    reportSection("Cert C", mSettings, mActiveCheckers, checkers::premiumCheckers, "Cert C: ");
    reportSection("Cert C++", mSettings, mActiveCheckers, checkers::premiumCheckers, "Cert C++: ");

    const int misraCVersion = getMisraCVersion(mSettings);

    if (misraCVersion == 0) {
        fout << std::endl << std::endl;
        fout << "Misra C" << std::endl;
        fout << "-------" << std::endl;
        fout << "Misra is not enabled" << std::endl;
    } else {
        fout << std::endl << std::endl;
        fout << "Misra C " << misraCVersion << std::endl;
        fout << "------------" << std::endl;
        for (const checkers::MisraInfo& info: checkers::misraC2012Directives) {
            const std::string directive = "Dir " + std::to_string(info.a) + "." + std::to_string(info.b);
            const bool active = isMisraRuleActive(mActiveCheckers, directive);
            fout << (active ? "Yes  " : "No   ") << "Misra C " << misraCVersion << ": " << directive;
            std::string extra;
            if (misraCVersion == 2012 && info.amendment >= 1)
                extra = " amendment:" + std::to_string(info.amendment);
            if (!extra.empty())
                fout << std::string(10 - directive.size(), ' ') << extra;
            fout << '\n';
        }
        for (const checkers::MisraInfo& info: checkers::misraC2012Rules) {
            const std::string rule = std::to_string(info.a) + "." + std::to_string(info.b);
            const bool active = isMisraRuleActive(mActiveCheckers, rule);
            fout << (active ? "Yes  " : "No   ") << "Misra C " << misraCVersion << ": " << rule;
            std::string extra;
            if (misraCVersion == 2012 && info.amendment >= 1)
                extra = " amendment:" + std::to_string(info.amendment);
            std::string reqs;
            if (info.amendment >= 3)
                reqs += ",premium";
            if (!active && !reqs.empty())
                extra += " require:" + reqs.substr(1);
            if (!extra.empty())
                fout << std::string(10 - rule.size(), ' ') << extra;
            fout << '\n';
        }
    }

    reportSection("Misra C++ 2008", mSettings, mActiveCheckers, checkers::premiumCheckers, "Misra C++ 2008: ");
    reportSection("Misra C++ 2023", mSettings, mActiveCheckers, checkers::premiumCheckers, "Misra C++ 2023: ");

    return fout.str();
}

std::string CheckersReport::getXmlReport(const std::string& criticalErrors) const
{
    std::string ret;
    if (!criticalErrors.empty())
        ret += "    <critical-errors>" + criticalErrors + "</critical-errors>\n";
    else
        ret += "    <critical-errors/>\n";
    ret += "    <checkers-report>\n";
    const int misraCVersion = getMisraCVersion(mSettings);
    for (std::string checker: mActiveCheckers) {
        if (checker.compare(0,8,"Misra C:") == 0)
            checker = "Misra C " + std::to_string(misraCVersion) + ":" + checker.substr(8);
        ret += "        <checker id=\"" + checker + "\"/>\n";
    }
    ret += "    </checkers-report>";
    return ret;
}
