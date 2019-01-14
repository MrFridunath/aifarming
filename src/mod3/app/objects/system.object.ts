import { Harvest } from './harvest.object'
export class System {
  id: number;
  name: string;
  harvest: Harvest
  ip: string;
  port: number;  
  username: string;
  password: string;
  phM: number;
  airTemperatureM: number;
  airHumidityM: number;
  soilHumidityM: number;
  rainM: number;
  lightM: number;
  lastWateringM: number;
}
