// Standalone tests for src/designchain.h. Build and run with tests/run_tests.ps1 (no game build needed).
#include "designchain.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace DesignChain;

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static bool hidden(const std::unordered_set<std::string> &set, const char *id) { return set.count(id) != 0; }

static const Node *find(const std::vector<Node> &nodes, const char *id)
{
	for (const Node &n : nodes) { if (n.id == id) { return &n; } }
	return nullptr;
}

static void testNewestUsableHidesOlder()
{
	// Python HPV <- Mantis HPV <- Tiger HPV
	std::vector<Node> n = {{"python", "", true}, {"mantis", "python", true}, {"tiger", "mantis", true}};
	auto h = supersededIds(n);
	CHECK(hidden(h, "python"));
	CHECK(hidden(h, "mantis"));
	CHECK(!hidden(h, "tiger"));
}

static void testFallsBackWhenNewerIsNotResearched()
{
	// Same line in a game where only the Python and Mantis bodies are researched.
	std::vector<Node> n = {{"python", "", true}, {"mantis", "python", true}, {"tiger", "mantis", false}};
	auto h = supersededIds(n);
	CHECK(hidden(h, "python"));
	CHECK(!hidden(h, "mantis"));
	CHECK(!hidden(h, "tiger"));

	// Only the oldest body is researched: the old design shows, nothing is hidden.
	n = {{"python", "", true}, {"mantis", "python", false}, {"tiger", "mantis", false}};
	h = supersededIds(n);
	CHECK(h.empty());
}

static void testUnusableMiddleLinkDoesNotBreakTheLine()
{
	// Tiger is available, Mantis is not (e.g. wrong factory size), Python is. Tiger still replaces Python.
	std::vector<Node> n = {{"python", "", true}, {"mantis", "python", false}, {"tiger", "mantis", true}};
	auto h = supersededIds(n);
	CHECK(hidden(h, "python"));
	CHECK(hidden(h, "mantis"));
	CHECK(!hidden(h, "tiger"));
}

static void testUnrelatedDesignsAreUntouched()
{
	std::vector<Node> n = {{"scout", "", true}, {"python", "", true}, {"mantis", "python", true}, {"", "", true}};
	auto h = supersededIds(n);
	CHECK(!hidden(h, "scout"));
	CHECK(hidden(h, "python"));
	CHECK(!hidden(h, "mantis"));
	CHECK(h.size() == 1);
}

static void testBranches()
{
	// Two different upgrades of the same design: either one being usable hides the original, and they do not hide each other.
	std::vector<Node> n = {{"base", "", true}, {"fast", "base", true}, {"tough", "base", false}};
	auto h = supersededIds(n);
	CHECK(hidden(h, "base"));
	CHECK(!hidden(h, "fast"));
	CHECK(!hidden(h, "tough"));
}

static void testDanglingLinkIsIgnored()
{
	std::vector<Node> n = {{"mantis", "deleted-long-ago", true}};
	auto h = supersededIds(n);
	CHECK(h.empty());
}

static void testLoopsDoNotHangAndAreRepaired()
{
	std::vector<Node> n = {{"a", "b", true}, {"b", "a", true}};
	auto h = supersededIds(n);  // must terminate
	(void)h;
	auto cleared = sanitize(n);
	CHECK(cleared.size() == 1);
	h = supersededIds(n);
	CHECK(h.size() == 1);  // exactly one of the two survives after repair

	std::vector<Node> self = {{"a", "a", true}};
	CHECK(sanitize(self).size() == 1);
	CHECK(self[0].supersedes.empty());
	CHECK(supersededIds(self).empty());

	std::vector<Node> longLoop = {{"a", "b", true}, {"b", "c", true}, {"c", "a", true}, {"d", "c", true}};
	sanitize(longLoop);
	// whatever was cleared, the result must be loop free
	for (const Node &node : longLoop) { CHECK(!wouldCreateCycle(longLoop, node.id, node.supersedes) || node.supersedes.empty()); }

	std::vector<Node> dangling = {{"a", "ghost", true}, {"b", "a", true}};
	auto c = sanitize(dangling);
	CHECK(c.size() == 1 && c[0] == "a");
	CHECK(find(dangling, "b")->supersedes == "a");
}

static void testWouldCreateCycle()
{
	std::vector<Node> n = {{"a", "", true}, {"b", "a", true}, {"c", "b", true}, {"x", "", true}};
	CHECK(wouldCreateCycle(n, "a", "c"));   // a would replace c, which already (transitively) replaces a
	CHECK(wouldCreateCycle(n, "a", "a"));
	CHECK(!wouldCreateCycle(n, "c", "x"));
	CHECK(!wouldCreateCycle(n, "x", "a"));
	CHECK(!wouldCreateCycle(n, "", "a"));
}

static void testSpliceOutKeepsLineConnected()
{
	std::vector<Node> n = {{"a", "", true}, {"b", "a", true}, {"c", "b", true}};
	auto relinked = spliceOut(n, "b");
	CHECK(relinked.size() == 1 && relinked[0] == "c");
	CHECK(find(n, "c")->supersedes == "a");
	CHECK(find(n, "b")->supersedes.empty());
	// removing the newest or an unknown design relinks nothing
	CHECK(spliceOut(n, "c").empty());
	CHECK(spliceOut(n, "nope").empty());
	CHECK(spliceOut(n, "").empty());

	// removing the root leaves the next design as the new root
	std::vector<Node> m = {{"a", "", true}, {"b", "a", true}};
	spliceOut(m, "a");
	CHECK(find(m, "b")->supersedes.empty());

	// after removal, the hidden set no longer includes the removed design's role
	std::vector<Node> k = {{"a", "", true}, {"b", "a", false}, {"c", "b", true}};
	spliceOut(k, "b");
	k.erase(std::remove_if(k.begin(), k.end(), [](const Node &x) { return x.id == "b"; }), k.end());
	auto h = supersededIds(k);
	CHECK(hidden(h, "a") && !hidden(h, "c"));
}

static void testNextVersionName()
{
	CHECK(nextVersionName("Tiger HPV") == "Tiger HPV 2");
	CHECK(nextVersionName("Tiger HPV 2") == "Tiger HPV 3");
	CHECK(nextVersionName("Tiger HPV 9") == "Tiger HPV 10");
	CHECK(nextVersionName("Tiger HPV 99") == "Tiger HPV 100");
	CHECK(nextVersionName("HPV75") == "HPV75 2");
	CHECK(nextVersionName("7") == "7 2");
	CHECK(nextVersionName(" 5") == " 5 2");
	CHECK(nextVersionName("") == "2");
	CHECK(nextVersionName("Tank 1234567") == "Tank 1234567 2");  // too many digits to be a version
	CHECK(nextVersionName("Truck 2000") == "Truck 2001");        // still readable as a version number
}

int main()
{
	testNewestUsableHidesOlder();
	testFallsBackWhenNewerIsNotResearched();
	testUnusableMiddleLinkDoesNotBreakTheLine();
	testUnrelatedDesignsAreUntouched();
	testBranches();
	testDanglingLinkIsIgnored();
	testLoopsDoNotHangAndAreRepaired();
	testWouldCreateCycle();
	testSpliceOutKeepsLineConnected();
	testNextVersionName();
	std::printf("%d checks, %d failures\n", checks, failures);
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
