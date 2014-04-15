#include <list>
#include "nlsr_fe.hpp"
#include "nlsr_nexthop.hpp"
#include "utility/nlsr_logger.hpp"

#define THIS_FILE "nlsr_fe.cpp"

namespace nlsr
{

  using namespace std;

  bool
  FibEntry::isEqualNextHops(Nhl& nhlOther)
  {
    if ( m_nhl.getSize() != nhlOther.getSize() )
    {
      return false;
    }
    else
    {
      int nhCount=0;
      std::list<NextHop>::iterator it1, it2;
      for ( it1=m_nhl.getNextHopList().begin(),
            it2 = nhlOther.getNextHopList().begin() ;
            it1 != m_nhl.getNextHopList().end() ; it1++, it2++)
      {
        if (it1->getConnectingFace() == it2->getConnectingFace() )
        {
          it1->setRouteCost(it2->getRouteCost());
          nhCount++;
        }
        else
        {
          break;
        }
      }
      return nhCount == m_nhl.getSize();
    }
  }

  ostream&
  operator<<(ostream& os, FibEntry fe)
  {
    os<<"Name Prefix: "<<fe.getName()<<endl;
    os<<"Time to Refresh: "<<fe.getTimeToRefresh()<<endl;
    os<<fe.getNhl()<<endl;
    return os;
  }

}//namespace nlsr