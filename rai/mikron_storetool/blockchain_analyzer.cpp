#include <rai/mikron_storetool/blockchain_analyzer.hpp>
#include <rai/mikron_storetool/block_helper.hpp>
#include <rai/node/node.hpp>
#include <random>

void rai::blockchain_analyzer::analyze_account_chain_length (boost::filesystem::path data_path)
{
	std::cout << "debug_blockchain_len " << std::endl;
	rai::inactive_node * node_a = new rai::inactive_node (data_path);
	node = node_a->node;
	transaction = new rai::transaction (node->store.environment, false);
	//blocks = pick_random_blocks (100, node);
	//long n = blocks.size ();
	//std::map<rai::block_hash, rai::span_info> account_spans;
	std::vector<long> account_chain_lengths;
	long cnt = 0;
	for (auto i (node->store.latest_begin (*transaction)), n (node->store.latest_end ()); i != n; ++i, ++cnt)
	{
		std::cerr << "i_cnt " << cnt << " "; // std::endl;
		rai::account account = rai::account (i->first);
		rai::account_info account_info = rai::account_info (i->second);
		rai::block_hash block_hash = account_info.head;
		long length = get_account_chain_length (block_hash);
		account_chain_lengths.push_back (length);
		std::cerr << std::endl << "i_cnt " << cnt << " len " << length << " bl " << account.to_account () << " " << block_hash.to_string () << std::endl;
		std::cout << cnt << " " << length << std::endl;
	}
	// computing median
	rai::blockchain_analyzer::printMedianStats ("Account chain lengths", account_chain_lengths);
}

void rai::blockchain_analyzer::analyze_account_chain_length_full (boost::filesystem::path data_path)
{
	std::cout << "debug_blockchain_len_full " << std::endl;
	rai::inactive_node * node_a = new rai::inactive_node (data_path);
	node = node_a->node;
	transaction = new rai::transaction (node->store.environment, false);
	auto frontiers (pick_random_frontiers (1000, node));
	long n = frontiers.size ();
	std::vector<long> account_chain_lengths;
	std::vector<long> account_chain_lengths_full;
	std::vector<long> account_counts;
	long cnt = 0;
	//for (auto i (node->store.latest_begin (*transaction)), n (node->store.latest_end ()); i != n; ++i, ++cnt)
	for (auto i (frontiers.begin ()), n (frontiers.end ()); i != n; ++i, ++cnt)
	{
		//std::cerr << "i_cnt " << cnt << std::endl;
		rai::account account = i->first;
		//rai::account_info account_info = rai::account_info (i->second);
		rai::block_hash block_hash = i->second;
		rai::chain_length len = get_account_chain_length_full (block_hash);
		account_chain_lengths.push_back (len.len);
		account_chain_lengths_full.push_back (len.len_full);
		account_counts.push_back (len.account_count);
		//std::cerr << std::endl << "i_cnt " << cnt << " len " << len.len << " l_f " << len.len_full << " bl " << account.to_account () << " " << block_hash.to_string () << std::endl;
		if (cnt % 20 == 0)
		{
			std::cerr << "cnt: " << cnt << std::endl;
			rai::blockchain_analyzer::printMedianStats ("Full account chain lengths", account_chain_lengths_full);
			rai::blockchain_analyzer::printMedianStats ("Account chain counts", account_counts);
		}
		std::cout << len.len << " " << len.len_full << std::endl;
	}
	// computing medians
	rai::blockchain_analyzer::printMedianStats ("Full account chain lengths", account_chain_lengths_full);
	rai::blockchain_analyzer::printMedianStats ("Account chain lengths", account_chain_lengths);
	rai::blockchain_analyzer::printMedianStats ("Account chain counts", account_counts);
}

long rai::blockchain_analyzer::get_account_chain_length (rai::block_hash hash)
{
	// Non-recursive iterative implementation (recursive does not work with large depths)
	if (hash.number () == 0)
	{
		return 0;
	}
	// current state
	rai::block_hash cur_hash (hash);
	long cur_len = 0;
	while (true)
	{
		if (cur_hash.number () == 0)
		{
			break;
		}
		auto block (node->store.block_get (*transaction, cur_hash));
		if (!block)
		{
			break;
		}
		// visit current block
		++cur_len;
		// move to previous
		cur_hash = block->previous ();
	}
	return cur_len;
}

rai::chain_length rai::blockchain_analyzer::get_account_chain_length_full (rai::block_hash hash)
{
	// Non-recursive iterative implementation (recursive does not work with large depths)
	rai::chain_length len;
	if (hash.number () == 0)
	{
		return len;
	}
	// current state
	rai::block_hash cur_hash (hash);
	len.len = 0;
	len.len_full = 0;
	len.account_count = 1;
	bool on_account_chain = true; // initiallz true, false as long as we are on ancestor chains
	while (true)
	{
		if (cur_hash.number () == 0)
		{
			break;
		}
		auto block (node->store.block_get (*transaction, cur_hash));
		if (!block)
		{
			break;
		}
		// visit current block
		if (on_account_chain)
		{
			++len.len;
		}
		++len.len_full;
		auto prev_hash (block->previous ());
		if (prev_hash.number () != 0)
		{
			// move to previous
			cur_hash = prev_hash;
			continue;
		}
		// no previous -- open_receive or genesis
		on_account_chain = false;
		std::cerr << " " << len.len_full << " +";
		// obtain receive from
		rai::block_helper::block_type block_type = rai::block_helper::get_block_type (block, node->ledger, *transaction);
		auto receive_from (rai::block_helper::get_receive_source (block, block_type));
		if (receive_from.number () != 0)
		{
			// move to ancestor chain
			++len.account_count;
			cur_hash = receive_from;
			continue;
		}
		// must be genesis block, stop
		break;
	}
	return len;
}

std::vector<std::pair<rai::account, rai::block_hash>> rai::blockchain_analyzer::pick_random_frontiers (int n, std::shared_ptr<rai::node> & node)
{
	// Eunmerate all frontiers, and randomly pick n out of them
	std::cerr << "Choosing " << n << " frontiers randomly..." << std::endl;
	std::vector<std::pair<rai::account, rai::block_hash>> all_frontiers;
	rai::transaction transaction (node->store.environment, false);
	for (auto i (node->store.latest_begin (transaction)), n (node->store.latest_end ()); i != n; ++i)
	{
		rai::account account = rai::account (i->first);
		rai::account_info account_info = rai::account_info (i->second);
		rai::block_hash hash = account_info.head;
		all_frontiers.push_back (std::make_pair (account, hash));
	}
	int n_all = all_frontiers.size ();
	std::default_random_engine generator;
	std::uniform_int_distribution<int> distribution(0, n_all - 1);
	std::vector<std::pair<rai::account, rai::block_hash>> frontiers;
	for (int i = 0; i < n; ++i)
	{
		int idx = distribution (generator);
		frontiers.push_back (all_frontiers[idx]);
	}
	std::cout << "Selected " << frontiers.size () << " frontiers from among " << all_frontiers.size () << std::endl;
	return frontiers;
}

void rai::blockchain_analyzer::printMedianStats (std::string const & prefix, std::vector<long> & numbers)
{
	std::sort (numbers.begin (), numbers.end ());
	auto n (numbers.size ());
	if (n == 0)
		return;
	auto min (numbers [0]);
	auto max (numbers [n - 1]);
	auto median (numbers [n / 2]);
	long sum = 0;
	for (long i = 0; i < n; ++i) sum += numbers[i];
	double avg = (double)sum / (double)n;
	std::cout << "Stats for " << prefix << ":" << std::endl;
	std::cout << "  cnt " << n << " range " << min << " -- " << max << " avg " << avg << "  median " << median << " sum " << sum << std::endl;
	std::cout << "  percentiles  0: " << min << "  1: " << numbers [(long)((double)n * 0.01)] << "  5: " << numbers [(long)((double)n * 0.05)] << "  10: " << numbers [(long)((double)n * 0.10)] << "  25: " << numbers [(long)((double)n * 0.25)] << "  50: " << median << "  75: " << numbers [(long)((double)n * 0.75)] << "  90: " << numbers [(long)((double)n * 0.90)] << "  95: " << numbers [(long)((double)n * 0.95)] << "  99: " << numbers [(long)((double)n * 0.99)] << "  100: " << max << std::endl;
}