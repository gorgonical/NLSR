#include "nlsr.hpp"
#include "lsdb.hpp"
#include "hello-protocol.hpp"
#include "utility/name-helper.hpp"

namespace nlsr {

const std::string HelloProtocol::INFO_COMPONENT="info";

void
HelloProtocol::expressInterest(const ndn::Name& interestName, uint32_t seconds)
{
  std::cout << "Expressing Interest :" << interestName << std::endl;
  ndn::Interest i(interestName);
  i.setInterestLifetime(ndn::time::seconds(seconds));
  i.setMustBeFresh(true);
  m_nlsr.getNlsrFace().expressInterest(i,
                                       ndn::bind(&HelloProtocol::processContent,
                                                 this,
                                                 _1, _2),
                                       ndn::bind(&HelloProtocol::processInterestTimedOut,
                                                 this, _1));
}

void
HelloProtocol::sendScheduledInterest(uint32_t seconds)
{
  std::list<Adjacent> adjList = m_nlsr.getAdjacencyList().getAdjList();
  for (std::list<Adjacent>::iterator it = adjList.begin(); it != adjList.end();
       ++it) {
    ndn::Name interestName = (*it).getName() ;
    interestName.append(INFO_COMPONENT);
    interestName.append(m_nlsr.getConfParameter().getRouterPrefix().wireEncode());
    expressInterest(interestName,
                    m_nlsr.getConfParameter().getInterestResendTime());
  }
  scheduleInterest(m_nlsr.getConfParameter().getInfoInterestInterval());
}

void
HelloProtocol::scheduleInterest(uint32_t seconds)
{
  m_nlsr.getScheduler().scheduleEvent(ndn::time::seconds(seconds),
                                      ndn::bind(&HelloProtocol::sendScheduledInterest,
                                                this, seconds));
}

void
HelloProtocol::processInterest(const ndn::Name& name,
                               const ndn::Interest& interest)
{
  const ndn::Name interestName = interest.getName();
  std::cout << "Interest Received for Name: " << interestName << std::endl;
  if (interestName.get(-2).toUri() != INFO_COMPONENT) {
    return;
  }
  ndn::Name neighbor;
  neighbor.wireDecode(interestName.get(-1).blockFromValue());
  std::cout << "Neighbor: " << neighbor << std::endl;
  if (m_nlsr.getAdjacencyList().isNeighbor(neighbor)) {
    ndn::Data data(ndn::Name(interest.getName()).appendVersion());
    data.setFreshnessPeriod(ndn::time::seconds(10)); // 10 sec
    data.setContent(reinterpret_cast<const uint8_t*>(INFO_COMPONENT.c_str()),
                    INFO_COMPONENT.size());
    m_keyChain.sign(data);
    std::cout << ">> D: " << data << std::endl;
    m_nlsr.getNlsrFace().put(data);
    int status = m_nlsr.getAdjacencyList().getStatusOfNeighbor(neighbor);
    if (status == 0) {
      ndn::Name interestName(neighbor);
      interestName.append(INFO_COMPONENT);
      interestName.append(m_nlsr.getConfParameter().getRouterPrefix());
      expressInterest(interestName,
                      m_nlsr.getConfParameter().getInterestResendTime());
    }
  }
}

void
HelloProtocol::processInterestTimedOut(const ndn::Interest& interest)
{
  const ndn::Name interestName(interest.getName());
  std::cout << "Interest timed out for Name: " << interestName << std::endl;
  if (interestName.get(-2).toUri() != INFO_COMPONENT) {
    return;
  }
  ndn::Name neighbor = interestName.getPrefix(-2);
  std::cout << "Neighbor: " << neighbor << std::endl;
  m_nlsr.getAdjacencyList().incrementTimedOutInterestCount(neighbor);
  int status = m_nlsr.getAdjacencyList().getStatusOfNeighbor(neighbor);
  uint32_t infoIntTimedOutCount =
    m_nlsr.getAdjacencyList().getTimedOutInterestCount(neighbor);
  std::cout << "Neighbor: " << neighbor << std::endl;
  std::cout << "Status: " << status << std::endl;
  std::cout << "Info Interest Timed out: " << infoIntTimedOutCount << std::endl;
  if ((infoIntTimedOutCount < m_nlsr.getConfParameter().getInterestRetryNumber())) {
    ndn::Name interestName(neighbor);
    interestName.append(INFO_COMPONENT);
    interestName.append(m_nlsr.getConfParameter().getRouterPrefix().wireEncode());
    expressInterest(interestName,
                    m_nlsr.getConfParameter().getInterestResendTime());
  }
  else if ((status == 1) &&
           (infoIntTimedOutCount == m_nlsr.getConfParameter().getInterestRetryNumber())) {
    m_nlsr.getAdjacencyList().setStatusOfNeighbor(neighbor, 0);
    m_nlsr.incrementAdjBuildCount();
    if (m_nlsr.getIsBuildAdjLsaSheduled() == false) {
      m_nlsr.setIsBuildAdjLsaSheduled(true);
      // event here
      m_nlsr.getScheduler().scheduleEvent(ndn::time::seconds(5),
                                          ndn::bind(&Lsdb::scheduledAdjLsaBuild,
                                                    &m_nlsr.getLsdb()));
    }
  }
}


void
HelloProtocol::processContent(const ndn::Interest& interest,
                              const ndn::Data& data)
{
  ndn::Name dataName = data.getName();
  std::cout << "Data received for name: " << dataName << std::endl;
  if (dataName.get(-3).toUri() == INFO_COMPONENT) {
    ndn::Name neighbor = dataName.getPrefix(-3);
    int oldStatus = m_nlsr.getAdjacencyList().getStatusOfNeighbor(neighbor);
    int infoIntTimedOutCount = m_nlsr.getAdjacencyList().getTimedOutInterestCount(
                                 neighbor);
    //debugging purpose start
    std::cout << "Before Updates: " << std::endl;
    std::cout << "Neighbor : " << neighbor << std::endl;
    std::cout << "Status: " << oldStatus << std::endl;
    std::cout << "Info Interest Timed out: " << infoIntTimedOutCount << std::endl;
    //debugging purpose end
    m_nlsr.getAdjacencyList().setStatusOfNeighbor(neighbor, 1);
    m_nlsr.getAdjacencyList().setTimedOutInterestCount(neighbor, 0);
    int newStatus = m_nlsr.getAdjacencyList().getStatusOfNeighbor(neighbor);
    infoIntTimedOutCount = m_nlsr.getAdjacencyList().getTimedOutInterestCount(
                             neighbor);
    //debugging purpose
    std::cout << "After Updates: " << std::endl;
    std::cout << "Neighbor : " << neighbor << std::endl;
    std::cout << "Status: " << newStatus << std::endl;
    std::cout << "Info Interest Timed out: " << infoIntTimedOutCount << std::endl;
    //debugging purpose end
    // change in Adjacency list
    if ((oldStatus - newStatus) != 0) {
      m_nlsr.incrementAdjBuildCount();
      /* Need to schedule event for Adjacency LSA building */
      if (m_nlsr.getIsBuildAdjLsaSheduled() == false) {
        m_nlsr.setIsBuildAdjLsaSheduled(true);
        // event here
        m_nlsr.getScheduler().scheduleEvent(ndn::time::seconds(5),
                                            ndn::bind(&Lsdb::scheduledAdjLsaBuild,
                                                      ndn::ref(m_nlsr.getLsdb())));
      }
    }
  }
}

} //namespace nlsr