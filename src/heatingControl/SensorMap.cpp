#include "SensorMap.h"
#include <arduino.h>

SensorMap::SensorMap(int maxSensorCount) {
  this->m_maxSensorCount = maxSensorCount;
  this->m_sensorCount = 0;
  this->m_sensorMapStorage = (SensorMapEntry **) malloc(maxSensorCount * sizeof (SensorMapEntry*));
  for(int i = 0; i < maxSensorCount; i++) {
    m_sensorMapStorage[i] = nullptr;
  }
}

SensorMap::~SensorMap() {
  this->clear();
  free (this->m_sensorMapStorage);
}

SensorMapEntry * SensorMap::findEntryByName(const String &name) const {
  for (int i = 0; i < m_sensorCount; i++) {
    SensorMapEntry * ep = m_sensorMapStorage[i];
    if (ep->name == name) return ep;
  }
  return nullptr;
}


SensorMapEntry * SensorMap::findEntryById(const String &id) const{
  for (int i = 0; i < m_sensorCount; i++) {
    SensorMapEntry * ep = m_sensorMapStorage[i];
    if (ep->id == id) return ep;
  }
  return nullptr;
}

const String &SensorMap::getNameForId(const String &id) const{
  SensorMapEntry * ep = this->findEntryById(id);
  if (!ep) return emptyString;
  return ep->name;
}

const String &SensorMap::getIdForName(const String &name) const{
  SensorMapEntry * ep = this->findEntryByName(name);
  if (!ep) return emptyString;
  return ep->id;
}

void SensorMap::setNameForId(const String &id, const String &name) {
  SensorMapEntry * ep = this->findEntryById(id);
  if (!ep) {
    if (this->m_sensorCount >= this->m_maxSensorCount) return;
    ep = new SensorMapEntry(id, name, this->m_sensorCount);
    this->m_sensorMapStorage[this->m_sensorCount] = ep;
    this->m_sensorCount++;
    m_changed = true;
  }
  else {
    if (ep->name == name) return; // No change
    ep->name = name;
    m_changed = true;
  }
}

void SensorMap::updateAtIndex(int index, const String &id, const String &name) {
  if (index < 0 || index >= this->m_maxSensorCount) return;
  // Fill any gaps up to index
  while (this->m_sensorCount < index) {
    this->m_sensorMapStorage[this->m_sensorCount] = new SensorMapEntry("", "", this->m_sensorCount);
    this->m_sensorCount++;
  }
  // If adding at the end, increase count
  if (m_sensorCount == index) {
    this->m_sensorCount++;
  }
  // Create a new entry if there isn't one already
  SensorMapEntry* ep = this->m_sensorMapStorage[index];
  if (!ep) {
    ep = new SensorMapEntry(id, name, index);
    this->m_sensorMapStorage[index] = ep;
    m_changed = true;
    return;
  }
  // Otherwise, update the existing entry
  if (ep->id != id) { 
    ep->id = id;
    m_changed = true;
  }
  if (ep->name != name) { 
    ep->name = name;
    m_changed = true;
  }
  return;
}

void SensorMap::removeId(const String &id) {
  SensorMapEntry * ep = this->findEntryById(id);
  this->removeEntry(ep);
}
void SensorMap::removeEntry(SensorMapEntry *ep) {
  // Copy the entries after the one to be deleted "down" in the array
  for(int i = ep->index+1; i < this->m_sensorCount; i++) {
    this->m_sensorMapStorage[i-1] = this->m_sensorMapStorage[i];
    this->m_sensorMapStorage[i-1]->index = i-1;
  }
  this->m_sensorMapStorage[this->m_sensorCount] = 0;
  this->m_sensorCount--;
  delete ep;
  m_changed = true;
}
void SensorMap::removeFromIndex(int index) {
  if (index < 0) return; // invalid
  if (index >= this->m_sensorCount) return;  // no change
  for (int i = index; i < this->m_sensorCount; i++) {
    SensorMapEntry * ep = this->m_sensorMapStorage[i];
    if (ep) delete ep;
    this->m_sensorMapStorage[i] = nullptr;
  }
  this->m_sensorCount = index;
  m_changed = true;
}

void SensorMap::clear() {
  for (int i = 0; i < m_sensorCount; i++) {
    SensorMapEntry *ep = this->m_sensorMapStorage[i];
    if (ep) delete ep;
    this->m_sensorMapStorage[i] = nullptr;
  }
  m_sensorCount = 0;
  m_changed = true;
}

void SensorMap::dump(Print &p) const{
  p.println("--------------------------------------------------------------");
  p.print(this->m_sensorCount);
  p.print(" of ");
  p.print(this->m_maxSensorCount);
  p.println(" entries used");
  p.println("--------------------------------------------------------------");
  for(int i = 0; i < m_maxSensorCount; i++) {
    SensorMapEntry * ep = this->m_sensorMapStorage[i];
    if (ep) {
      p.print((long)ep, HEX);
      p.print(" (");
      p.print(ep->index);
      p.print("): ");

      if (ep->id) {
        p.print("'");
        p.print(ep->id); 
        p.print("'");
      }
      else {
        p.print("(null)");
      }

      p.print(" -> ");

      if (ep->name) {
        p.print("'");
        p.print(ep->name); 
        p.print("'");
      }
      else {
        p.print("(null)");
      }
      p.println();
    }
    else {
      p.println("00000000");
    }
  }
  p.println("--------------------------------------------------------------");
}



