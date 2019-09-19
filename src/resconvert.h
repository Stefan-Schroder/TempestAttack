#include <map>

using namespace std;

namespace tmpst{

    static map<string, pair<int, int>> resMap = {
            {"640x400@85",	    {832,445}},
            {"720x400@85",	    {936,446}},
		    {"640x480@60",	    {800,525}},
		    {"640x480@100",	    {848,509}},
		    {"640x480@72",	    {832,520}},
		    {"640x480@75",	    {840,500}},
		    {"640x480@85",	    {832,509}},
		    {"768x576@60",	    {976,597}},
		    {"768x576@72",	    {992,601}},
		    {"768x576@75",	    {1008,602}},
		    {"768x576@85",	    {1008,605}},
		    {"768x576@100",	    {1024,611}},
		    {"800x600@56",	    {1024,625}},
		    {"800x600@60",	    {1056,628}},
		    {"800x600@72",	    {1040,666}},
		    {"800x600@75",	    {1056,625}},
		    {"800x600@85",	    {1048,631}},
		    {"800x600@100",	    {1072,636}},
		    {"1024x600@60",	    {1312,622}},
		    {"1024x768i@43",	{1264,817}},
		    {"1024x768@60",	    {1344,806}},
		    {"1024x768@70",	    {1328,806}},
		    {"1024x768@75",	    {1312,800}},
		    {"1024x768@85",	    {1376,808}},
		    {"1024x768@100",	{1392,814}},
		    {"1024x768@120",	{1408,823}},
		    {"1152x864@60",	    {1520,895}},
		    {"1152x864@75",	    {1600,900}},
		    {"1152x864@85",	    {1552,907}},
		    {"1152x864@100",	{1568,915}},
		    {"1280x768@60",	    {1680,795}},
		    {"1280x800@60",	    {1680,828}},
		    {"1280x960@60",	    {1800,1000}},
		    {"1280x960@75",	    {1728,1002}},
		    {"1280x960@85",	    {1728,1011}},
		    {"1280x960@100",	{1760,1017}},
		    {"1280x1024@60",	{1688,1066}},
		    {"1280x1024@75",	{1688,1066}},
		    {"1280x1024@85",	{1728,1072}},
		    {"1280x1024@100",   {1760,1085}},
		    {"1280x1024@120",   {1776,1097}},
		    {"1368x768@60",	    {1800,795}},
		    {"1400x1050@60",	{1880,1082}},
		    {"1400x1050@72",	{1896,1094}},
		    {"1400x1050@75",	{1896,1096}},
		    {"1400x1050@85",	{1912,1103}},
		    {"1400x1050@100",   {1928,1112}},
		    {"1440x900@60",	    {1904,932}},
		    {"1440x1050@60",	{1936,1087}},
		    {"1600x1000@60",	{2144,1035}},
		    {"1600x1000@75",	{2160,1044}},
		    {"1600x1000@85",	{2176,1050}},
		    {"1600x1000@100",   {2192,1059}},
		    {"1600x1024@60",	{2144,1060}},
		    {"1600x1024@75",	{2176,1069}},
		    {"1600x1024@76",	{2096,1070}},
		    {"1600x1024@85",	{2176,1075}},
		    {"1600x1200@60",	{2160,1250}},
		    {"1600x1200@65",	{2160,1250}},
		    {"1600x1200@70",	{2160,1250}},
		    {"1600x1200@75",	{2160,1250}},
		    {"1600x1200@85",	{2160,1250}},
		    {"1600x1200@100",   {2208,1271}},
		    {"1680x1050@60",	{2256,1087}},
		    {"1792x1344@60",	{2448,1394}},
		    {"1792x1344@75",	{2456,1417}},
		    {"1856x1392@60",	{2528,1439}},
		    {"1856x1392@75",	{2560,1500}},
		    {"1920x1080@60",	{2576,1125}},
		    {"1920x1080@75",	{2608,1126}},
		    {"1920x1200@60",	{2592,1242}},
		    {"1920x1200@75",	{2624,1253}},
		    {"1920x1440@60",	{2600,1500}},
		    {"1920x1440@75",	{2640,1500}},
		    {"1920x2400@25",	{2048,2434}},
		    {"1920x2400@30",	{2044,2434}},
		    {"2048x1536@60",	{2800,1589}}
    };

    int getWidth(string res, double refresh){
        string round_ref = to_string(round(refresh));
        string search_string = res+"@"+round_ref.substr(0,round_ref.find("."));
        return resMap[search_string].first;
    }

    int getHeight(string res, double refresh){
        string round_ref = to_string(round(refresh));
        string search_string = res+"@"+round_ref.substr(0,round_ref.find("."));
        return resMap[search_string].second;
    }
}